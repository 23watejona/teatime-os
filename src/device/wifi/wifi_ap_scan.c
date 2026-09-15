#include "uart.h"
#include "wifi_frame.h"

#define BEACON_IES (MAC_HDR_LEN + BEACON_FIXED)

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

static const char *rsn_security(const volatile unsigned char *ie, unsigned int len) {
    unsigned int end = IE_HDR_LEN + len;
    unsigned int p = IE_HDR_LEN + RSN_VERSION_LEN + RSN_SUITE_LEN;
    unsigned int pairwise_count = ie[p] | (ie[p + 1] << 8);
    unsigned int ccmp = 0;
    unsigned int q = p + RSN_COUNT_LEN;
    for (unsigned int k = 0; k < pairwise_count && q + RSN_SUITE_LEN <= end; k++, q += RSN_SUITE_LEN)
        if (ie[q + 3] == RSN_CIPHER_CCMP) ccmp = 1;
    unsigned int akm_count = (q + RSN_COUNT_LEN <= end) ? (ie[q] | (ie[q + 1] << 8)) : 0;
    unsigned int a = q + RSN_COUNT_LEN, sae = 0, psk = 0;
    for (unsigned int k = 0; k < akm_count && a + RSN_SUITE_LEN <= end; k++, a += RSN_SUITE_LEN) {
        if (ie[a + 3] == RSN_AKM_SAE) sae = 1;
        else if (ie[a + 3] == RSN_AKM_PSK || ie[a + 3] == RSN_AKM_PSK_SHA256) psk = 1;
    }
    return sae ? (psk ? "wpa2/3-psk/sae" : "wpa3-sae")
               : (ccmp ? "wpa2-psk-ccmp" : "wpa-psk-tkip");
}

void wifi_ap_observe(volatile unsigned char *buf, unsigned int buflen) {
    if (buflen < RXCTRL_LEN + BEACON_IES)
        return;

    int rssi = (signed char) buf[0];
    volatile unsigned char *f = buf + RXCTRL_LEN;
    volatile struct mac_header *h = (volatile struct mac_header *) f;
    unsigned int flen = buflen - RXCTRL_LEN;

    unsigned int fc0 = h->frame_control[0];
    unsigned int subtype = FC_SUBTYPE(fc0);
    if (FC_TYPE(fc0) != FC_TYPE_MGMT || (subtype != MGMT_BEACON && subtype != MGMT_PROBE_RESP))
        return;

    volatile unsigned char *bssid = h->addr3;
    // a known target still falls through, so a rescan can republish its channel
    int known = bssid_known(bssid);
    if (known && !is_target_bssid(bssid))
        return;

    const volatile unsigned char *ssid = 0;
    unsigned int ssid_len = 0;
    int channel = -1;
    const char *sec = "open";
    unsigned int off = BEACON_IES;
    while (off + IE_HDR_LEN <= flen) {
        unsigned int tag = f[off];
        unsigned int len = f[off + 1];
        if (off + IE_HDR_LEN + len > flen)
            break;
        if (tag == IE_SSID) {
            ssid = &f[off + IE_HDR_LEN];
            ssid_len = len;
        } else if (tag == IE_DS_PARAMS && len >= 1) {
            channel = f[off + IE_HDR_LEN];
        } else if (tag == IE_RSN && len >= RSN_VERSION_LEN + RSN_SUITE_LEN + RSN_COUNT_LEN) {
            sec = rsn_security(f + off, len);
        }
        off += IE_HDR_LEN + len;
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
