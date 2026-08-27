#include "reg_util.h"
#include "uart.h"
#include "string.h"
#include "wifi_tx.h"
#include "wifi_wpa.h"
#include "wifi_crypto.h"
#include "wifi_ccmp.h"

// tx hands the mac plaintext behind a ccmp header and the mac fills the mic; rx comes back decrypted in place but still carrying the ccmp header and mic, so wifi_ccmp_rx strips them
#define CCMP_ENGINE  0x00030103u

extern unsigned char ap_bssid[6];
extern unsigned char wifi_mac_addr[6];

#define KEY_SLOT(s)  (0x3ff21400u + (s) * 0x28u)
#define KEY_ENABLE   0x3ff2080cu

static void write_key(unsigned int slot, unsigned int flagword, const u8 *key) {
    unsigned int lo = ap_bssid[0] | (ap_bssid[1] << 8) |
                      (ap_bssid[2] << 16) | (ap_bssid[3] << 24);
    unsigned int hi = ap_bssid[4] | (ap_bssid[5] << 8);
    WRITE_REG(KEY_SLOT(slot) + 0, lo);
    WRITE_REG(KEY_SLOT(slot) + 4, flagword | hi);
    for (int i = 0; i < 4; i++)
        WRITE_REG(KEY_SLOT(slot) + 8 + i * 4,
                  key[i*4] | (key[i*4+1] << 8) | (key[i*4+2] << 16) | (key[i*4+3] << 24));
    WRITE_REG(KEY_ENABLE, READ_REG(KEY_ENABLE) | (1u << slot));
}

/* 48-bit CCMP packet number for the pairwise TK. Must strictly increase per
   MPDU under one key or the AP discards frames as replays; starts at 1. */
static unsigned int pn_lo = 1, pn_hi;

void wifi_ccmp_install_keys(void) {
    /* The slot number, not the flag word, selects the key class the HW routes a
       frame to: a group frame (multicast A1) draws its key from a group-class slot
       (2-5) matched on A2=BSSID, a unicast frame from a pairwise-class slot (6-7).
       So the GTK goes in slot 2 and the PTK in slot 6; swap them and the HW decrypts
       group frames with the pairwise TK. */
    write_key(2, 0x40cc0000u, wpa_gtk); /* group key -> group-class slot 2 */
    write_key(6, 0x004c0000u, wpa_tk); /* pairwise key -> pairwise-class slot 6 */

    /* Datapath crypto engine, enabled after the slots: HW encrypt on TX, decrypt on RX. */
    WRITE_REG(0x3ff20800, CCMP_ENGINE);

    /* A fresh TK starts a fresh PN space. */
    pn_lo = 1;
    pn_hi = 0;
    kprintf_uart("ccmp: keys installed (enable=%x eng=%x)\n",
                 READ_REG(KEY_ENABLE), READ_REG(0x3ff20800));
}

void wifi_ccmp_install_gtk(void) {
    write_key(2, 0x40cc0000u, wpa_gtk);
}

void wifi_ccmp_clear_keys(void) {
    WRITE_REG(KEY_ENABLE, READ_REG(KEY_ENABLE) & ~((1u << 2) | (1u << 6)));
}

static unsigned int tx_seq;

/* Build and transmit a CCMP data frame carrying `payload` to `da`. The MAC
   encrypts the payload and appends the MIC in hardware. */
int wifi_ccmp_tx(const unsigned char *da, const unsigned char *payload, unsigned int len) {
    u8 f[1600];
    if (32 + len + 8 > sizeof(f))
        return -1;

    f[0] = 0x08; f[1] = 0x41; /* Protected + ToDS */
    f[2] = 0x00; f[3] = 0x00;
    memcpy(f + 4, ap_bssid, 6);
    memcpy(f + 10, wifi_mac_addr, 6);
    memcpy(f + 16, da, 6);
    /* Distinct seq per frame, or the AP drops repeats as duplicate retransmits. */
    f[22] = (tx_seq << 4) & 0xf0;
    f[23] = (tx_seq >> 4) & 0xff;
    tx_seq = (tx_seq + 1) & 0xfff;

    /* Hand the MAC plaintext — it encrypts in place, so a pre-encrypted payload
       comes out double-processed and the AP drops it. */
    f[24] = pn_lo;        f[25] = pn_lo >> 8;  f[26] = 0x00; f[27] = 0x20;
    f[28] = pn_lo >> 16;  f[29] = pn_lo >> 24; f[30] = pn_hi; f[31] = pn_hi >> 8;
    memcpy(f + 32, payload, len);
    memset(f + 32 + len, 0, 8); /* MIC space; HW fills it */
    if (++pn_lo == 0)
        pn_hi++;
    return wifi_tx_frame(f, 32 + len + 8);
}

int wifi_ccmp_rx(volatile unsigned char *buf, unsigned int len, unsigned char *out) {
    if (len < 12 + 24)
        return -1;
    volatile u8 *f = buf + 12;
    // the dma descriptor length undercounts the frame, so the real 802.11 length comes from the rxcontrol header: word0 bits 16-27 for legacy rates, word1 bits 8-23 when the ht flag in word0 bits 14-15 is set
    unsigned int w0 = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
    unsigned int w1 = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
    unsigned int flen = (w0 & 0xc000) ? ((w1 >> 8) & 0xffff) : ((w0 >> 16) & 0xfff);
    if (flen < 36 || flen > 1600)
        return -1;

    unsigned int fc0 = f[0], fc1 = f[1];
    if (((fc0 >> 2) & 3) != 2)
        return -1;
    for (int i = 0; i < 6; i++)
        if (f[10 + i] != ap_bssid[i])
            return -1;
    if (!(fc1 & 0x40))
        return -1;

    unsigned int subtype = (fc0 >> 4) & 0xf;
    unsigned int mh = (subtype & 8) ? 26 : 24;

    unsigned int hdr = mh + 8;
    if (flen <= hdr + 8)
        return -1;
    unsigned int plen = flen - hdr - 8;
    if (plen > 2048)
        plen = 2048;
    memcpy(out, (const void *)(f + hdr), plen);
    return (int) plen;
}
