#include "reg_util.h"
#include "uart.h"
#include "string.h"
#include "wifi_crypto.h"
#include "wifi_frame.h"
#include "wifi_tx.h"
#include "wifi_wpa.h"
#include "wifi_ccmp.h"
#include "net.h"
#include "ap_secrets.h"
#include "rng.h"

extern unsigned char ap_bssid[6];
extern const char ap_ssid[];
extern unsigned char wifi_mac_addr[6];

static const char passphrase[] = AP_PASS;

volatile int wpa_state;
unsigned char wpa_tk[16];
unsigned char wpa_gtk[32];
unsigned int  wpa_gtk_len;
unsigned int  wpa_gtk_id;

static u8 pmk[32];
static u8 ptk[48];
static u8 anonce[32], snonce[32];
static u8 replay[8];

#define KCK  (ptk)
#define KEK  (ptk + 16)
#define TK   (ptk + 32)

#define EAPOL_VERSION  1
#define EAPOL_TYPE_KEY 3
#define EAPOL_HDR_LEN  4
#define KEY_DESC_RSN   2

#define E_VERSION   0
#define E_TYPE      1
#define E_BODYLEN   2
#define E_DESC      4
#define E_KEYINFO   5
#define E_KEYLEN    7
#define E_REPLAY    9
#define E_NONCE     17
#define E_MIC       81
#define E_KDLEN     97
#define E_KEYDATA   99

#define KI_PAIRWISE 0x0008
#define KI_INSTALL  0x0040
#define KI_ACK      0x0080
#define KI_MIC      0x0100
#define KI_SECURE   0x0200
#define KI_ENCDATA  0x1000

// the same ie goes in the association request and in msg2 of the 4-way, so the ap sees one consistent cipher choice
const unsigned char rsn_ie[RSN_IE_LEN] = {
    IE_RSN, RSN_IE_LEN - IE_HDR_LEN,
    RSN_VERSION, 0x00,
    RSN_SUITE(RSN_CIPHER_CCMP),
    0x01, 0x00, RSN_SUITE(RSN_CIPHER_CCMP),
    0x01, 0x00, RSN_SUITE(RSN_AKM_PSK),
    0x00, 0x00,
};

void wpa_prep(void) {
    wpa2_pmk(passphrase, (const u8 *)ap_ssid, strlen(ap_ssid), pmk);
    kprintf_uart("wpa: PMK ready\n");
}

void wpa_begin(void) {
    if (wpa_state != WPA_IDLE)
        return;
    rng_fill(snonce, 32);
    wpa_state = WPA_WAIT_M1;
    kprintf_uart("wpa: armed, waiting for EAPOL msg1\n");
}

static void derive_ptk(void) {
    u8 data[76];
    const u8 *a = ap_bssid, *b = wifi_mac_addr;
    const u8 *lo = (memcmp(a, b, 6) < 0) ? a : b;
    const u8 *hi = (lo == a) ? b : a;
    memcpy(data, lo, 6);
    memcpy(data + 6, hi, 6);

    const u8 *nlo = (memcmp(anonce, snonce, 32) < 0) ? anonce : snonce;
    const u8 *nhi = (nlo == anonce) ? snonce : anonce;
    memcpy(data + 12, nlo, 32);
    memcpy(data + 44, nhi, 32);

    sha1_prf(pmk, 32, "Pairwise key expansion", data, 76, ptk, 48);
}

static void send_eapol(unsigned int keyinfo, const u8 *nonce,
                       const u8 *keydata, unsigned int kdlen) {
    u8 f[256];
    struct mac_header *mac = (struct mac_header *)f;
    memset(mac, 0, sizeof(*mac));
    mac->frame_control[0] = FC_DATA;
    mac->frame_control[1] = FC_TO_DS;
    memcpy(mac->addr1, ap_bssid, 6);
    memcpy(mac->addr2, wifi_mac_addr, 6);
    memcpy(mac->addr3, ap_bssid, 6);

    struct llc_snap *llc = (struct llc_snap *)(f + MAC_HDR_LEN);
    memset(llc, 0, sizeof(*llc));
    llc->dsap = LLC_SAP_SNAP;
    llc->ssap = LLC_SAP_SNAP;
    llc->control = LLC_CONTROL_UI;
    llc->ethertype[0] = ETHERTYPE_EAPOL >> 8;
    llc->ethertype[1] = ETHERTYPE_EAPOL & 0xff;

    unsigned int n = MAC_HDR_LEN + LLC_SNAP_LEN;
    u8 *e = f + n;
    unsigned int blen = E_KEYDATA - EAPOL_HDR_LEN + kdlen;
    memset(e, 0, E_KEYDATA + kdlen);
    e[E_VERSION] = EAPOL_VERSION;
    e[E_TYPE] = EAPOL_TYPE_KEY;
    e[E_BODYLEN] = blen >> 8; e[E_BODYLEN + 1] = blen;
    e[E_DESC] = KEY_DESC_RSN;
    e[E_KEYINFO] = keyinfo >> 8; e[E_KEYINFO + 1] = keyinfo;
    e[E_KEYLEN] = 0x00; e[E_KEYLEN + 1] = kdlen ? 16 : 0;
    memcpy(e + E_REPLAY, replay, 8);
    if (nonce)
        memcpy(e + E_NONCE, nonce, 32);
    e[E_KDLEN] = kdlen >> 8; e[E_KDLEN + 1] = kdlen;
    if (kdlen)
        memcpy(e + E_KEYDATA, keydata, kdlen);

    u8 mic[SHA1_DIGEST];
    hmac_sha1(KCK, 16, e, E_KEYDATA + kdlen, mic);
    memcpy(e + E_MIC, mic, 16);

    n += E_KEYDATA + kdlen;
    // before WPA_DONE no key is installed so the 4-way goes out in the clear, but a group-rekey reply must ride the encrypted link like any other data
    if (wpa_state == WPA_DONE)
        wifi_ccmp_tx(ap_bssid, f + MAC_HDR_LEN, n - MAC_HDR_LEN);
    else
        wifi_tx_frame(f, n);
}

static volatile u8 *find_eapol(volatile u8 *f, unsigned int flen) {
    unsigned int fc0 = f[0];
    if (FC_TYPE(fc0) != FC_TYPE_DATA)
        return 0;
    unsigned int hdr = MAC_HDR_LEN;
    if (FC_SUBTYPE(fc0) & SUBTYPE_QOS)
        hdr += QOS_CTRL_LEN;
    if (flen < hdr + LLC_SNAP_LEN + EAPOL_HDR_LEN)
        return 0;
    volatile struct llc_snap *llc = (volatile struct llc_snap *)(f + hdr);
    if (llc->dsap != LLC_SAP_SNAP || llc->ssap != LLC_SAP_SNAP || llc->control != LLC_CONTROL_UI ||
        ((llc->ethertype[0] << 8) | llc->ethertype[1]) != ETHERTYPE_EAPOL)
        return 0;
    return f + hdr + LLC_SNAP_LEN;
}

static int from_our_ap(volatile u8 *f) {
    volatile struct mac_header *h = (volatile struct mac_header *)f;
    for (int i = 0; i < 6; i++)
        if (h->addr1[i] != wifi_mac_addr[i])
            return 0;
    for (int i = 0; i < 6; i++)
        if (h->addr2[i] != ap_bssid[i])
            return 0;
    return 1;
}

static int mic_ok(volatile u8 *e, unsigned int elen) {
    u8 rx_mic[16], calc[SHA1_DIGEST], tmp[256];
    if (elen > sizeof(tmp))
        return 0;
    memcpy(tmp, (const void *)e, elen);
    memcpy(rx_mic, tmp + E_MIC, 16);
    memset(tmp + E_MIC, 0, 16);
    hmac_sha1(KCK, 16, tmp, elen, calc);
    return memcmp(calc, rx_mic, 16) == 0;
}

static void extract_gtk(const u8 *kd, unsigned int kdlen) {
    unsigned int off = 0;
    while (off + 2 <= kdlen) {
        unsigned int tag = kd[off], len = kd[off + 1];
        if (off + 2 + len > kdlen)
            break;
        if (tag == 0xdd && len >= 6 &&
            kd[off + 2] == 0x00 && kd[off + 3] == 0x0f &&
            kd[off + 4] == 0xac && kd[off + 5] == 0x01) {
            wpa_gtk_id = kd[off + 6] & 0x03;
            wpa_gtk_len = len - 6;
            if (wpa_gtk_len > sizeof(wpa_gtk))
                wpa_gtk_len = sizeof(wpa_gtk);
            memcpy(wpa_gtk, kd + off + 8, wpa_gtk_len);
            return;
        }
        off += 2 + len;
    }
}

static void handle_m1(volatile u8 *e) {
    memcpy(replay, (const void *)(e + E_REPLAY), 8);
    memcpy(anonce, (const void *)(e + E_NONCE), 32);
    derive_ptk();
    send_eapol(2 | KI_PAIRWISE | KI_MIC, snonce, rsn_ie, sizeof(rsn_ie));
    wpa_state = WPA_WAIT_M3;
    kprintf_uart("wpa: msg1 rx, msg2 sent\n");
}

static void handle_m3(volatile u8 *e, unsigned int elen) {
    if (!mic_ok(e, elen)) {
        kprintf_uart("wpa: msg3 MIC FAIL\n");
        return;
    }
    memcpy(replay, (const void *)(e + E_REPLAY), 8);

    unsigned int kdlen = (e[E_KDLEN] << 8) | e[E_KDLEN + 1];
    if (kdlen >= 16 && (kdlen % 8) == 0) {
        u8 wrapped[256], plain[256];
        if (kdlen <= sizeof(wrapped)) {
            memcpy(wrapped, (const void *)(e + E_KEYDATA), kdlen);
            if (aes_unwrap(KEK, 16, kdlen / 8 - 1, wrapped, plain) == 0)
                extract_gtk(plain, kdlen - 8);
        }
    }

    memcpy(wpa_tk, TK, 16);

    // once the pairwise key is installed the hardware encrypts every frame to the ap, so a lost msg4 could never be resent readably; burst it before installing so one lands in the clear
    if (wpa_state != WPA_DONE) {
        for (int i = 0; i < 4; i++)
            send_eapol(2 | KI_PAIRWISE | KI_MIC | KI_SECURE, 0, 0, 0);
        wifi_ccmp_install_keys();
        wpa_state = WPA_DONE;
        kprintf_uart("wpa: msg3 MIC ok, msg4 sent — 4-way COMPLETE (gtk_len=%u id=%u)\n",
                     wpa_gtk_len, wpa_gtk_id);
    }
}

static void handle_group_m1(volatile u8 *e, unsigned int elen) {
    if (!mic_ok(e, elen)) {
        kprintf_uart("wpa: group msg1 MIC FAIL\n");
        return;
    }
    memcpy(replay, (const void *)(e + E_REPLAY), 8);

    unsigned int kdlen = (e[E_KDLEN] << 8) | e[E_KDLEN + 1];
    if (kdlen < 16 || (kdlen % 8) != 0 || kdlen > 256 || E_KEYDATA + kdlen > elen)
        return;
    u8 wrapped[256], plain[256];
    memcpy(wrapped, (const void *)(e + E_KEYDATA), kdlen);
    if (aes_unwrap(KEK, 16, kdlen / 8 - 1, wrapped, plain) != 0)
        return;
    extract_gtk(plain, kdlen - 8);
    wifi_ccmp_install_gtk();

    send_eapol(2 | KI_MIC | KI_SECURE, 0, 0, 0);
    kprintf_uart("wpa: GTK rekeyed (len=%u id=%u), msg2 sent\n", wpa_gtk_len, wpa_gtk_id);
}

void wifi_wpa_eapol(unsigned char *llc, unsigned int len) {
    if (wpa_state != WPA_DONE)
        return;
    if (len < LLC_SNAP_LEN + E_KEYDATA)
        return;
    u8 *e = llc + LLC_SNAP_LEN;
    if (e[E_TYPE] != EAPOL_TYPE_KEY)
        return;
    unsigned int ki = (e[E_KEYINFO] << 8) | e[E_KEYINFO + 1];
    unsigned int elen = EAPOL_HDR_LEN + ((e[E_BODYLEN] << 8) | e[E_BODYLEN + 1]);
    if (LLC_SNAP_LEN + elen > len)
        return;
    if ((ki & (KI_PAIRWISE | KI_MIC | KI_ACK)) == (KI_MIC | KI_ACK))
        handle_group_m1(e, elen);
}

void wpa_reset(void) {
    wpa_state = WPA_IDLE;
}

void wifi_wpa_input(volatile unsigned char *buf, unsigned int len) {
    if (wpa_state == WPA_IDLE)
        return;
    if (len < RXCTRL_LEN + MAC_HDR_LEN)
        return;
    volatile u8 *f = buf + RXCTRL_LEN;
    unsigned int flen = len - RXCTRL_LEN;

    volatile u8 *e = find_eapol(f, flen);
    if (!e || !from_our_ap(f))
        return;
    if (e[E_TYPE] != EAPOL_TYPE_KEY)
        return;

    unsigned int ki = (e[E_KEYINFO] << 8) | e[E_KEYINFO + 1];
    unsigned int elen = EAPOL_HDR_LEN + ((e[E_BODYLEN] << 8) | e[E_BODYLEN + 1]);
    if (RXCTRL_LEN + (unsigned int)(e - f) + elen > len)
        return;

    // a lost msg2 or msg4 makes the ap retransmit msg1 or msg3, so both are handled again after their reply rather than gated on state
    if (ki & KI_MIC) {
        if ((ki & KI_INSTALL) && wpa_state != WPA_WAIT_M1)
            handle_m3(e, elen);
    } else if (ki & KI_ACK) {
        handle_m1(e);
    }
}
