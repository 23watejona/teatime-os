#include "ap_secrets.h"
#include "timer.h"
#include "reg_util.h"
#include "uart.h"
#include "wifi_tx.h"
#include "wifi_sta.h"
#include "wifi_wpa.h"
#include "ap_secrets.h"

extern int wifi_locked_channel;
extern unsigned char wifi_mac_addr[6];

#define AP_CHANNEL     6
#define RETRY_PERIOD   24000000u   /* ~300 ms at 80 MHz */

const unsigned char ap_bssid[6] = { AP_BSSID_BYTES };
const char ap_ssid[] = AP_SSID;

volatile int wifi_sta_state;
volatile unsigned int wifi_sta_aid;

static unsigned int last_tx;
static unsigned int seq;

// with both address-match units masking every byte the mac acks nothing and the ap abandons the join, so one unit is pointed at our mac and one at the bssid with a full mask
static void program_rx_filter(void) {
    const unsigned char *m = wifi_mac_addr;
    WRITE_REG(0x3ff20c48, m[0] | (m[1] << 8) | (m[2] << 16) | (m[3] << 24));
    WRITE_REG(0x3ff20c4c, m[4] | (m[5] << 8));
    WRITE_REG(0x3ff20c58, 0xffffffff);
    WRITE_REG(0x3ff20c5c, 0x0000ffff);
    WRITE_REG(0x3ff20c5c, READ_REG(0x3ff20c5c) | 0x00010000);

    WRITE_REG(0x3ff20c3c, READ_REG(0x3ff20c3c) & 0xfffeffff);
    WRITE_REG(0x3ff20c28, ap_bssid[0] | (ap_bssid[1] << 8) | (ap_bssid[2] << 16) | (ap_bssid[3] << 24));
    WRITE_REG(0x3ff20c2c, ap_bssid[4] | (ap_bssid[5] << 8));
    WRITE_REG(0x3ff20c38, 0xffffffff);
    WRITE_REG(0x3ff20c3c, 0x0000ffff);
    WRITE_REG(0x3ff20c3c, READ_REG(0x3ff20c3c) | 0x00010000);

    // in sniffer mode the mac acks nothing and truncates data frames to the header, so the sniffer bits from bring-up are undone here
    WRITE_REG_UNMASK(0x3ff20c18, 0x0000000c);
    WRITE_REG_UNMASK(0x3ff20800, 0x03000000);
    WRITE_REG_MASK(0x3ff20800, 0x00010000);
    WRITE_REG_UNMASK(0x3ff20804, 0x03000000);
    WRITE_REG_MASK(0x3ff20804, 0x00010000);
    WRITE_REG_UNMASK(0x3ff20c88, 0x00040000);
    WRITE_REG_MASK(0x3ff20c94, 0x00000001);
    WRITE_REG_MASK(0x60009d44, 0x24000000);
    WRITE_REG_MASK(0x3ff2006c, 0x00000007);
}

static unsigned int put_mgmt_hdr(unsigned char *b, unsigned int subtype) {
    unsigned int n = 0;
    b[n++] = (subtype << 4);
    b[n++] = 0x00;
    b[n++] = 0x00; b[n++] = 0x00;                          /* duration */
    for (int i = 0; i < 6; i++) b[n++] = ap_bssid[i];      /* addr1 DA */
    for (int i = 0; i < 6; i++) b[n++] = wifi_mac_addr[i]; /* addr2 SA */
    for (int i = 0; i < 6; i++) b[n++] = ap_bssid[i];      /* addr3 BSSID */
    b[n++] = (seq << 4) & 0xf0;
    b[n++] = (seq >> 4) & 0xff;
    seq = (seq + 1) & 0xfff;
    return n;
}

static void send_auth(void) {
    unsigned char f[64];
    unsigned int n = put_mgmt_hdr(f, 11);
    f[n++] = 0x00; f[n++] = 0x00;   /* algorithm: open system */
    f[n++] = 0x01; f[n++] = 0x00;   /* transaction sequence 1 */
    f[n++] = 0x00; f[n++] = 0x00;   /* status */
    wifi_tx_frame(f, n);
}

static const unsigned char rsn_ie[] = {
    0x30, 0x14,
    0x01, 0x00,
    0x00, 0x0f, 0xac, 0x04,         /* group cipher CCMP */
    0x01, 0x00, 0x00, 0x0f, 0xac, 0x04,  /* pairwise CCMP */
    0x01, 0x00, 0x00, 0x0f, 0xac, 0x02,  /* AKM PSK */
    0x00, 0x00,
};

static void send_assoc(void) {
    unsigned char f[96];
    unsigned int n = put_mgmt_hdr(f, 0);
    f[n++] = 0x31; f[n++] = 0x04;   /* capability: ESS, privacy, short preamble/slot */
    f[n++] = 0x03; f[n++] = 0x00;   /* listen interval */

    unsigned int slen = sizeof(ap_ssid) - 1;
    f[n++] = 0x00; f[n++] = slen;
    for (unsigned int i = 0; i < slen; i++) f[n++] = ap_ssid[i];

    f[n++] = 0x01; f[n++] = 0x08;
    f[n++] = 0x82; f[n++] = 0x84; f[n++] = 0x8b; f[n++] = 0x96;
    f[n++] = 0x24; f[n++] = 0x30; f[n++] = 0x48; f[n++] = 0x6c;

    for (unsigned int i = 0; i < sizeof(rsn_ie); i++) f[n++] = rsn_ie[i];
    wifi_tx_frame(f, n);
}

void wifi_station_tick(void) {
    unsigned int now = ccount();

    switch (wifi_sta_state) {
    case STA_INIT:
        wifi_locked_channel = AP_CHANNEL;
        program_rx_filter();
        wpa_prep();                     /* slow PBKDF2 now, before msg1 is in flight */
        wifi_sta_state = STA_AUTH;
        last_tx = now - RETRY_PERIOD;   /* authenticate on the next tick */
        return;
    case STA_AUTH:
        if (now - last_tx < RETRY_PERIOD)
            return;
        last_tx = now;
        send_auth();
        return;
    case STA_ASSOC:
        if (now - last_tx < RETRY_PERIOD)
            return;
        last_tx = now;
        send_assoc();
        return;
    default:
        return;
    }
}

static int from_our_ap(volatile unsigned char *f) {
    for (int i = 0; i < 6; i++)
        if (f[4 + i] != wifi_mac_addr[i])
            return 0;
    for (int i = 0; i < 6; i++)
        if (f[10 + i] != ap_bssid[i])
            return 0;
    return 1;
}

void wifi_sta_input(volatile unsigned char *buf, unsigned int len) {
    if (len < 12 + 30)
        return;
    volatile unsigned char *f = buf + 12;

    unsigned int fc0 = f[0];
    if (((fc0 >> 2) & 3) != 0)
        return;
    if (!from_our_ap(f))
        return;

    unsigned int subtype = (fc0 >> 4) & 0xf;
    unsigned int body = 24;

    if (subtype == 11) {
        unsigned int aseq = f[body + 2] | (f[body + 3] << 8);
        unsigned int status = f[body + 4] | (f[body + 5] << 8);
        if (aseq != 2)
            return;
        if (status != 0) {
            kprintf_uart("sta: auth rejected status=%u\n", status);
            return;
        }
        if (wifi_sta_state == STA_AUTH) {
            wifi_sta_state = STA_ASSOC;
            last_tx = ccount() - RETRY_PERIOD;
            kprintf_uart("sta: authenticated, associating\n");
        }
    } else if (subtype == 1) {
        unsigned int status = f[body + 2] | (f[body + 3] << 8);
        unsigned int aid = (f[body + 4] | (f[body + 5] << 8)) & 0x3fff;
        if (status != 0) {
            kprintf_uart("sta: assoc rejected status=%u\n", status);
            return;
        }
        if (wifi_sta_state == STA_ASSOC) {
            wifi_sta_aid = aid;
            wifi_sta_state = STA_RUN;
            kprintf_uart("sta: ASSOCIATED aid=%u\n", aid);
            wpa_begin();
        }
    }
}
