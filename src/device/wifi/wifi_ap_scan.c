#include "uart.h"

#define RXCTRL_LEN   12
#define MAC_HDR_LEN  24
#define BEACON_FIXED 12
#define TAG_OFFSET   (MAC_HDR_LEN + BEACON_FIXED)

#define MAX_APS 256

extern unsigned char ap_bssid[6];
volatile int wifi_target_channel;

static unsigned char seen_bssid[MAX_APS][6];
static int ap_count;
static int ap_table_full;

static int bssid_known(const volatile unsigned char *b) {
    for (int i = 0; i < ap_count; i++) {
        int same = 1;
        for (int j = 0; j < 6; j++)
            if (seen_bssid[i][j] != b[j]) { same = 0; break; }
        if (same)
            return 1;
    }
    return ap_table_full;
}

static void bssid_remember(const volatile unsigned char *b) {
    if (ap_count >= MAX_APS) {
        ap_table_full = 1;
        return;
    }
    for (int j = 0; j < 6; j++)
        seen_bssid[ap_count][j] = b[j];
    ap_count++;
}

static int is_target_bssid(const volatile unsigned char *b) {
    for (int i = 0; i < 6; i++)
        if (b[i] != ap_bssid[i])
            return 0;
    return 1;
}

void wifi_ap_observe(volatile unsigned char *buf, unsigned int buflen) {
    if (buflen < RXCTRL_LEN + TAG_OFFSET)
        return;

    int rssi = (signed char) buf[0];
    volatile unsigned char *f = buf + RXCTRL_LEN;
    unsigned int flen = buflen - RXCTRL_LEN;

    unsigned int fc0 = f[0];
    unsigned int type = (fc0 >> 2) & 3;
    unsigned int subtype = (fc0 >> 4) & 0xf;
    if (type != 0 || (subtype != 8 && subtype != 5))
        return;

    volatile unsigned char *bssid = &f[16];
    // a known target still falls through, so a rescan can republish its channel
    int known = bssid_known(bssid);
    if (known && !is_target_bssid(bssid))
        return;

    const volatile unsigned char *ssid = 0;
    unsigned int ssid_len = 0;
    int channel = -1;
    const char *sec = "open";
    unsigned int off = TAG_OFFSET;
    while (off + 2 <= flen) {
        unsigned int tag = f[off];
        unsigned int len = f[off + 1];
        if (off + 2 + len > flen)
            break;
        if (tag == 0) {
            ssid = &f[off + 2];
            ssid_len = len;
        } else if (tag == 3 && len >= 1) {
            channel = f[off + 2];
        } else if (tag == 48 && len >= 8) {
            unsigned int p = off + 2 + 2 + 4;
            unsigned int pn = f[p] | (f[p + 1] << 8);
            unsigned int pw_ccmp = 0;
            unsigned int q = p + 2;
            for (unsigned int k = 0; k < pn && q + 4 <= off + 2 + len; k++, q += 4)
                if (f[q + 3] == 4) pw_ccmp = 1;
            unsigned int an = (q + 2 <= off + 2 + len) ? (f[q] | (f[q + 1] << 8)) : 0;
            unsigned int a = q + 2, sae = 0, psk = 0;
            for (unsigned int k = 0; k < an && a + 4 <= off + 2 + len; k++, a += 4) {
                if (f[a + 3] == 8) sae = 1;
                else if (f[a + 3] == 2 || f[a + 3] == 6) psk = 1;
            }
            sec = sae ? (psk ? "wpa2/3-psk/sae" : "wpa3-sae")
                      : (pw_ccmp ? "wpa2-psk-ccmp" : "wpa-psk-tkip");
        }
        off += 2 + len;
    }

    if (is_target_bssid(bssid) && channel > 0)
        wifi_target_channel = channel;

    if (known)
        return;

    bssid_remember(bssid);

    kprintf_uart("AP #%d  \"", ap_count);
    if (ssid_len == 0) {
        kprintf_uart("<hidden>");
    } else {
        for (unsigned int i = 0; i < ssid_len && i < 32; i++) {
            unsigned char c = ssid[i];
            kputc_uart((c >= 0x20 && c < 0x7f) ? c : '.');
        }
    }
    kprintf_uart("\"  %x:%x:%x:%x:%x:%x  ch=%d  rssi=%d  %s\n",
                 bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
                 channel, rssi, sec);
}
