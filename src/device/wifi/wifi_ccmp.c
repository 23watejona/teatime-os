#include "reg_util.h"
#include "wifi_regs.h"
#include "uart.h"
#include "string.h"
#include "wifi_frame.h"
#include "wifi_tx.h"
#include "wifi_wpa.h"
#include "wifi_crypto.h"
#include "wifi_ccmp.h"

// tx hands the mac plaintext behind a ccmp header and the mac fills the mic; rx comes back decrypted in place but still carrying the ccmp header and mic, so wifi_ccmp_rx strips them

extern unsigned char ap_bssid[6];
extern unsigned char wifi_mac_addr[6];

// the hardware looks for group keys in one slot range and the pairwise key in the other, so each slot's flag word must carry the class code for its range
#define GROUP_KEY_SLOT    2
#define PAIRWISE_KEY_SLOT 6

static void write_key(unsigned int slot, unsigned int flags, const u8 *key) {
    unsigned int lo = ap_bssid[0] | (ap_bssid[1] << 8) |
                      (ap_bssid[2] << 16) | (ap_bssid[3] << 24);
    unsigned int hi = ap_bssid[4] | (ap_bssid[5] << 8);
    WRITE_REG(MAC_KEY_ADDR(slot), lo);
    WRITE_REG(MAC_KEY_FLAGS(slot), flags | hi);
    for (int w = 0; w < 4; w++)
        WRITE_REG(MAC_KEY_MATERIAL(slot, w),
                  key[w*4] | (key[w*4+1] << 8) | (key[w*4+2] << 16) | (key[w*4+3] << 24));
    WRITE_REG_MASK(MAC_KEY_ENABLE, 1u << slot);
}

// a group frame is matched to its entry by the key id in the ccmp header, so a rekey rewrites the same slot with the new id
static void write_gtk(void) {
    write_key(GROUP_KEY_SLOT, MAC_KEY_CIPHER_CCMP | MAC_KEY_GROUP_CLASS | (wpa_gtk_id << MAC_KEY_ID_SHIFT), wpa_gtk);
}

void wifi_ccmp_install_keys(void) {
    write_gtk();
    write_key(PAIRWISE_KEY_SLOT, MAC_KEY_CIPHER_CCMP | MAC_KEY_PAIRWISE_CLASS, wpa_tk);

    WRITE_REG(MAC_CRYPTO_CIPHER, MAC_CIPHER_CCMP);

    kprintf_uart("ccmp: keys installed (enable=%x eng=%x)\n",
                 READ_REG(MAC_KEY_ENABLE), READ_REG(MAC_CRYPTO_CIPHER));
}

void wifi_ccmp_install_gtk(void) {
    write_gtk();
}

void wifi_ccmp_clear_keys(void) {
    WRITE_REG_UNMASK(MAC_KEY_ENABLE, (1u << GROUP_KEY_SLOT) | (1u << PAIRWISE_KEY_SLOT));
}

struct ccmp_frame {
    struct mac_header mac;
    struct ccmp_header ccmp;
    unsigned char body[];
} __attribute__((packed));

int wifi_ccmp_tx(const unsigned char *da, const unsigned char *payload, unsigned int len) {
    u8 f[MAX_FRAME_LEN];
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
    if (len < RXCTRL_LEN + MAC_HDR_LEN)
        return -1;
    volatile u8 *f = buf + RXCTRL_LEN;
    volatile struct mac_header *h = (volatile struct mac_header *)f;
    // the dma descriptor length undercounts the frame, so the real 802.11 length comes from the rxcontrol header: word0 bits 16-27 for legacy rates, word1 bits 8-23 when the ht flag in word0 bits 14-15 is set
    unsigned int w0 = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
    unsigned int w1 = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
    unsigned int flen = (w0 & 0xc000) ? ((w1 >> 8) & 0xffff) : ((w0 >> 16) & 0xfff);
    if (flen > MAX_FRAME_LEN)
        return -1;

    unsigned int fc0 = h->frame_control[0];
    if (FC_TYPE(fc0) != FC_TYPE_DATA)
        return -1;
    for (int i = 0; i < 6; i++)
        if (h->addr2[i] != ap_bssid[i])
            return -1;
    if (!(h->frame_control[1] & FC_PROTECTED))
        return -1;

    unsigned int hdr = MAC_HDR_LEN + CCMP_HDR_LEN;
    if (FC_SUBTYPE(fc0) & SUBTYPE_QOS)
        hdr += QOS_CTRL_LEN;
    if (flen <= hdr + CCMP_MIC_LEN)
        return -1;
    unsigned int plen = flen - hdr - CCMP_MIC_LEN;
    if (plen > 2048)
        plen = 2048;
    memcpy(out, (const void *)(f + hdr), plen);
    return (int) plen;
}
