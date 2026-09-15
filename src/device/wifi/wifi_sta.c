#include "timer.h"
#include "proc.h"
#include "reg_util.h"
#include "wifi_regs.h"
#include "uart.h"
#include "string.h"
#include "wifi_frame.h"
#include "wifi_tx.h"
#include "wifi_sta.h"
#include "wifi_wpa.h"
#include "wifi_ccmp.h"
#include "ap_secrets.h"

extern int wifi_locked_channel;
extern unsigned char wifi_mac_addr[6];
extern volatile int wifi_target_channel;

#define RETRY_PERIOD (3 * TICKS_PER_SEC / 10)
#define BEACON_LOSS (5 * TICKS_PER_SEC)
#define WPA_STALL (10 * TICKS_PER_SEC)

extern unsigned char ap_bssid[6];
const char ap_ssid[] = AP_SSID;

volatile int wifi_sta_state;
volatile unsigned int wifi_sta_aid;

static unsigned int last_tx;
static unsigned int last_heard;
static unsigned int run_since;

// beacon loss means the ap may have moved channel or vanished, so the lock is dropped and the scanner hunts again
static void link_down(int rescan) {
    wifi_ccmp_clear_keys();
    wpa_reset();
    wifi_sta_aid = 0;
    if (rescan) {
        wifi_target_channel = 0;
        wifi_locked_channel = 0;
        wifi_sta_state = STA_INIT;
    } else {
        wifi_sta_state = STA_AUTH;
        last_tx = ticks() - RETRY_PERIOD;
    }
}

// with both address-match units masking every byte the mac acks nothing and the ap abandons the join, so one unit is pointed at our mac and one at the bssid with a full mask
static void program_rx_filter(void) {
    const unsigned char *m = wifi_mac_addr;
    WRITE_REG(MAC_ADDR_LO(0), m[0] | (m[1] << 8) | (m[2] << 16) | (m[3] << 24));
    WRITE_REG(MAC_ADDR_HI(0), m[4] | (m[5] << 8));
    WRITE_REG(MAC_ADDR_MASK_LO(0), 0xffffffff);
    WRITE_REG(MAC_ADDR_MASK_HI(0), 0x0000ffff);
    WRITE_REG(MAC_BSSID_LO(0), ap_bssid[0] | (ap_bssid[1] << 8) | (ap_bssid[2] << 16) | (ap_bssid[3] << 24));
    WRITE_REG(MAC_BSSID_HI(0), ap_bssid[4] | (ap_bssid[5] << 8));
    WRITE_REG(MAC_BSSID_MASK_LO(0), 0xffffffff);
    WRITE_REG(MAC_BSSID_MASK_HI(0), 0x0000ffff);
    WRITE_REG_MASK(MAC_ADDR_MASK_HI(0), MAC_ADDR_MATCH_ENABLE);
    WRITE_REG_MASK(MAC_BSSID_MASK_HI(0), MAC_ADDR_MATCH_ENABLE);

    // in sniffer mode the mac acks nothing and truncates data frames to the header, so the sniffer bits from bring-up are undone here: sniffer events off, crypto pass-through off and the engine on
    WRITE_REG_UNMASK(MAC_INT_ENABLE, WDEV_SNIFFER_EVENT);
    WRITE_REG_UNMASK(MAC_CRYPTO_CIPHER, 0x03000000);
    WRITE_REG_MASK(MAC_CRYPTO_CIPHER, 0x00010000);
    WRITE_REG_UNMASK(MAC_CRYPTO_CONF, 0x03000000);
    WRITE_REG_MASK(MAC_CRYPTO_CONF, 0x00010000);
    // raw delivery stays on even though the vendor clears it on sniffer exit: with it clear the mac drops every protected frame instead of decrypting, observed on hardware
    WRITE_REG_MASK(MAC_RX_OPTION, 0x00040000);
    // automatic ack back on, the baseband's normal rx mode bits back, and all three address filters enabled
    WRITE_REG_MASK(MAC_TX_OPTION, 0x00000001);
    WRITE_REG_MASK(0x60009d44, 0x24000000);
    WRITE_REG_MASK(MAC_RX_FILTER, 0x00000007);
}

static unsigned int put_mgmt_hdr(unsigned char *b, unsigned int subtype) {
    struct mac_header *mac = (struct mac_header *)b;
    memset(mac, 0, sizeof(*mac));
    mac->frame_control[0] = subtype << FC_SUBTYPE_SHIFT;
    memcpy(mac->addr1, ap_bssid, 6);
    memcpy(mac->addr2, wifi_mac_addr, 6);
    memcpy(mac->addr3, ap_bssid, 6);
    return sizeof(*mac);
}

static unsigned int put_ie(unsigned char *b, unsigned int id,
                           const unsigned char *data, unsigned int len) {
    b[0] = id;
    b[1] = len;
    memcpy(b + IE_HDR_LEN, data, len);
    return IE_HDR_LEN + len;
}

static void send_auth(void) {
    unsigned char f[64];
    unsigned int n = put_mgmt_hdr(f, MGMT_AUTH);
    struct auth_body *auth = (struct auth_body *)(f + n);
    auth->algorithm = AUTH_ALGO_OPEN;
    auth->sequence = AUTH_SEQ_REQUEST;
    auth->status = 0;
    n += sizeof(*auth);
    wifi_tx_frame(f, n);
}

static const unsigned char supported_rates[] = {
    RATE_BASIC | RATE_KBPS(1000), RATE_BASIC | RATE_KBPS(2000),
    RATE_BASIC | RATE_KBPS(5500), RATE_BASIC | RATE_KBPS(11000),
    RATE_KBPS(18000), RATE_KBPS(24000), RATE_KBPS(36000), RATE_KBPS(54000),
};

static void send_assoc(void) {
    unsigned char f[96];
    unsigned int n = put_mgmt_hdr(f, MGMT_ASSOC_REQ);
    struct assoc_req_body *assoc = (struct assoc_req_body *)(f + n);
    assoc->capability = CAP_ESS | CAP_PRIVACY | CAP_SHORT_PREAMBLE | CAP_SHORT_SLOT;
    assoc->listen_interval = 3;
    n += sizeof(*assoc);

    n += put_ie(f + n, IE_SSID, (const unsigned char *)ap_ssid, sizeof(ap_ssid) - 1);
    n += put_ie(f + n, IE_SUPPORTED_RATES, supported_rates, sizeof(supported_rates));
    memcpy(f + n, rsn_ie, RSN_IE_LEN);
    n += RSN_IE_LEN;
    wifi_tx_frame(f, n);
}

void wifi_station_tick(void) {
    unsigned int now = ticks();

    switch (wifi_sta_state) {
    case STA_INIT:
        if (wifi_target_channel <= 0)
            return;
        wifi_locked_channel = wifi_target_channel;
        program_rx_filter();
        kprintf_uart("sta: locking channel %d\n", wifi_locked_channel);
        wifi_sta_state = STA_AUTH;
        last_tx = now - RETRY_PERIOD;
        return;
    case STA_AUTH:
    case STA_ASSOC:
        if (now - last_heard >= BEACON_LOSS) {
            kprintf_uart("sta: beacon loss, rescanning\n");
            link_down(1);
            return;
        }
        if (now - last_tx < RETRY_PERIOD)
            return;
        last_tx = now;
        if (wifi_sta_state == STA_AUTH)
            send_auth();
        else
            send_assoc();
        return;
    case STA_RUN:
        if (now - last_heard >= BEACON_LOSS) {
            kprintf_uart("sta: beacon loss, rescanning\n");
            link_down(1);
            return;
        }
        // the 4-way only advances on ap retransmits, so once the ap gives up only a fresh auth restarts it
        if (wpa_state != WPA_DONE && now - run_since >= WPA_STALL) {
            kprintf_uart("sta: handshake stalled, re-authenticating\n");
            link_down(0);
        }
        return;
    default:
        return;
    }
}

static int from_our_ap(volatile struct mac_header *h) {
    for (int i = 0; i < 6; i++)
        if (h->addr1[i] != wifi_mac_addr[i])
            return 0;
    for (int i = 0; i < 6; i++)
        if (h->addr2[i] != ap_bssid[i])
            return 0;
    return 1;
}

void wifi_sta_input(volatile unsigned char *buf, unsigned int len) {
    if (len < RXCTRL_LEN + MAC_HDR_LEN + sizeof(struct assoc_resp_body))
        return;
    volatile struct mac_header *h = (volatile struct mac_header *)(buf + RXCTRL_LEN);
    volatile unsigned char *body = (volatile unsigned char *)h + MAC_HDR_LEN;

    // beacons are broadcast and fail the directed check below, so liveness is tracked on the transmitter address alone
    int a2_ours = 1;
    for (int i = 0; i < 6; i++)
        if (h->addr2[i] != ap_bssid[i])
            a2_ours = 0;
    if (a2_ours)
        last_heard = ticks();

    unsigned int fc0 = h->frame_control[0];
    if (FC_TYPE(fc0) != FC_TYPE_MGMT)
        return;
    if (!from_our_ap(h))
        return;

    unsigned int subtype = FC_SUBTYPE(fc0);

    if (subtype == MGMT_DEAUTH || subtype == MGMT_DISASSOC) {
        volatile struct reason_body *r = (volatile struct reason_body *)body;
        kprintf_uart("sta: %s reason=%u, re-authenticating\n",
                     subtype == MGMT_DEAUTH ? "DEAUTH" : "DISASSOC", r->reason);
        if (wifi_sta_state != STA_INIT)
            link_down(0);
        return;
    }

    if (subtype == MGMT_AUTH) {
        volatile struct auth_body *auth = (volatile struct auth_body *)body;
        unsigned int status = auth->status;
        if (auth->sequence != AUTH_SEQ_RESPONSE)
            return;
        if (status != 0) {
            kprintf_uart("sta: auth rejected status=%u\n", status);
            return;
        }
        if (wifi_sta_state == STA_AUTH) {
            wifi_sta_state = STA_ASSOC;
            last_tx = ticks() - RETRY_PERIOD;
            kprintf_uart("sta: authenticated, associating\n");
        }
    } else if (subtype == MGMT_ASSOC_RESP) {
        volatile struct assoc_resp_body *assoc = (volatile struct assoc_resp_body *)body;
        unsigned int status = assoc->status;
        unsigned int aid = assoc->aid & AID_MASK;
        if (status != 0) {
            kprintf_uart("sta: assoc rejected status=%u\n", status);
            return;
        }
        if (wifi_sta_state == STA_ASSOC) {
            wifi_sta_aid = aid;
            wifi_sta_state = STA_RUN;
            last_heard = ticks();
            run_since = last_heard;
            kprintf_uart("sta: ASSOCIATED aid=%u\n", aid);
            wpa_begin();
        }
    }
}
