#include "reg_util.h"
#include "uart.h"
#include "string.h"
#include "wifi_crypto.h"
#include "wifi_tx.h"
#include "wifi_wpa.h"
#include "wifi_ccmp.h"
#include "ap_secrets.h"

extern unsigned char ap_bssid[6];
extern const char ap_ssid[];
extern unsigned char wifi_mac_addr[6];

static const char passphrase[] = AP_PASS;

/* ESP8266 hardware RNG. */
#define WDEV_RNG   0x3ff20e44u

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

static const u8 rsn_ie[] = {
    0x30, 0x14, 0x01, 0x00,
    0x00, 0x0f, 0xac, 0x04,
    0x01, 0x00, 0x00, 0x0f, 0xac, 0x04,
    0x01, 0x00, 0x00, 0x0f, 0xac, 0x02,
    0x00, 0x00,
};

static int pmk_ready;

void wpa_prep(void) {
    if (pmk_ready)
        return;
    wpa2_pmk(passphrase, (const u8 *)ap_ssid, strlen(ap_ssid), pmk);
    pmk_ready = 1;
    kprintf_uart("wpa: PMK ready\n");
}

void wpa_begin(void) {
    if (wpa_state != WPA_IDLE)
        return;
    wpa_prep();
    for (int i = 0; i < 32; i += 4) {
        unsigned int r = READ_REG(WDEV_RNG);
        snonce[i] = r; snonce[i+1] = r >> 8; snonce[i+2] = r >> 16; snonce[i+3] = r >> 24;
    }
    wpa_state = WPA_WAIT_M1;
    kprintf_uart("wpa: armed, waiting for EAPOL msg1\n");
}

/* PTK = PRF-384 over the address/nonce pair in canonical (min||max) order. */
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
    unsigned int n = 0;

    f[n++] = 0x08; f[n++] = 0x01;
    f[n++] = 0x00; f[n++] = 0x00;
    for (int i = 0; i < 6; i++) f[n++] = ap_bssid[i];
    for (int i = 0; i < 6; i++) f[n++] = wifi_mac_addr[i];
    for (int i = 0; i < 6; i++) f[n++] = ap_bssid[i];
    f[n++] = 0x00; f[n++] = 0x00;

    f[n++] = 0xaa; f[n++] = 0xaa; f[n++] = 0x03;
    f[n++] = 0x00; f[n++] = 0x00; f[n++] = 0x00;
    f[n++] = 0x88; f[n++] = 0x8e;

    u8 *e = f + n;
    unsigned int blen = 95 + kdlen;
    memset(e, 0, 99 + kdlen);
    e[E_VERSION] = 0x01;
    e[E_TYPE] = 0x03;
    e[E_BODYLEN] = blen >> 8; e[E_BODYLEN + 1] = blen;
    e[E_DESC] = 0x02;
    e[E_KEYINFO] = keyinfo >> 8; e[E_KEYINFO + 1] = keyinfo;
    e[E_KEYLEN] = 0x00; e[E_KEYLEN + 1] = kdlen ? 16 : 0;
    memcpy(e + E_REPLAY, replay, 8);
    if (nonce)
        memcpy(e + E_NONCE, nonce, 32);
    e[E_KDLEN] = kdlen >> 8; e[E_KDLEN + 1] = kdlen;
    if (kdlen)
        memcpy(e + E_KEYDATA, keydata, kdlen);

    u8 mic[SHA1_DIGEST];
    hmac_sha1(KCK, 16, e, 99 + kdlen, mic);
    memcpy(e + E_MIC, mic, 16);

    n += 99 + kdlen;
    wifi_tx_frame(f, n);
}

static volatile u8 *find_eapol(volatile u8 *f, unsigned int flen) {
    unsigned int fc0 = f[0];
    if (((fc0 >> 2) & 3) != 2)
        return 0;
    unsigned int hdr = 24;
    if ((fc0 >> 4) & 8)
        hdr += 2;
    if (flen < hdr + 8 + 4)
        return 0;
    volatile u8 *s = f + hdr;
    if (s[0] != 0xaa || s[1] != 0xaa || s[2] != 0x03 ||
        s[6] != 0x88 || s[7] != 0x8e)
        return 0;
    return s + 8;
}

static int from_our_ap(volatile u8 *f) {
    for (int i = 0; i < 6; i++)
        if (f[4 + i] != wifi_mac_addr[i])
            return 0;
    for (int i = 0; i < 6; i++)
        if (f[10 + i] != ap_bssid[i])
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

void wifi_wpa_input(volatile unsigned char *buf, unsigned int len) {
    if (wpa_state == WPA_IDLE)
        return;
    if (len < 12 + 24)
        return;
    volatile u8 *f = buf + 12;
    unsigned int flen = len - 12;

    volatile u8 *e = find_eapol(f, flen);
    if (!e || !from_our_ap(f))
        return;
    if (e[E_TYPE] != 0x03)
        return;

    unsigned int ki = (e[E_KEYINFO] << 8) | e[E_KEYINFO + 1];
    unsigned int elen = 4 + ((e[E_BODYLEN] << 8) | e[E_BODYLEN + 1]);
    if (12 + (unsigned int)(e - f) + elen > len)
        return;

    // a lost msg2 or msg4 makes the ap retransmit msg1 or msg3, so both are handled again after their reply rather than gated on state
    if (ki & KI_MIC) {
        if ((ki & KI_INSTALL) && wpa_state != WPA_WAIT_M1)
            handle_m3(e, elen);
    } else if (ki & KI_ACK) {
        handle_m1(e);
    }
}
