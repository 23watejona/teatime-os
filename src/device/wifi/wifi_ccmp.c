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

void wifi_ccmp_install_keys(void) {
    // the slot number, not the flag word, picks the key class: group frames draw from slots 2-5 matched on a2=bssid and unicast from 6-7, so swapping them makes the hw decrypt group frames with the pairwise key
    write_key(2, 0x004c0000u | ((wpa_gtk_id & 1) << 24), wpa_gtk);
    write_key(6, 0x004c0000u, wpa_tk);

    WRITE_REG(0x3ff20800, CCMP_ENGINE);

    kprintf_uart("ccmp: keys installed (enable=%x eng=%x)\n",
                 READ_REG(KEY_ENABLE), READ_REG(0x3ff20800));
}

void wifi_ccmp_install_gtk(void) {
    write_key(2, 0x004c0000u | ((wpa_gtk_id & 1) << 24), wpa_gtk);
}

void wifi_ccmp_clear_keys(void) {
    WRITE_REG(KEY_ENABLE, READ_REG(KEY_ENABLE) & ~((1u << 2) | (1u << 6)));
}

struct ccmp_frame {
    struct mac_header mac;
    struct ccmp_header ccmp;
    unsigned char body[];
} __attribute__((packed));

int wifi_ccmp_tx(const unsigned char *da, const unsigned char *payload, unsigned int len) {
    u8 f[1600];
    struct ccmp_frame *frame = (struct ccmp_frame *)f;
    unsigned int frame_len = sizeof(*frame) + len + CCMP_MIC_LEN;
    if (frame_len > sizeof(f))
        return -1;

    memset(frame, 0, sizeof(*frame));
    frame->mac.frame_control[0] = FC_DATA;
    frame->mac.frame_control[1] = FC_PROTECTED | FC_TO_DS;
    memcpy(frame->mac.addr1, ap_bssid, 6);
    memcpy(frame->mac.addr2, wifi_mac_addr, 6);
    memcpy(frame->mac.addr3, da, 6);
    memcpy(frame->body, payload, len);
    memset(frame->body + len, 0, CCMP_MIC_LEN); // the mac writes the mic, so only the space is reserved
    return wifi_tx_frame(f, frame_len);
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
