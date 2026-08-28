#include "reg_util.h"
#include "wait.h"
#include "uart.h"
#include "wifi_dma.h"
#include "wifi_tx.h"

// tx is a per-queue mmio block rather than a descriptor ring: the length, plcp and descriptor words are armed first and the go bits set last, so the queue never launches a half-written frame; completion is TX_DONE_BIT in MAC_INT_EVENT

#define TXQ(q)         (0x3ff20dc0u - 0x18u * (q))
#define MAC_INT_EVENT  0x3ff20c20u
#define MAC_INT_CLEAR  0x3ff20c24u
#define TX_DONE_BIT    0x00080000u
#define TX_GO_BITS     0xc0000000u

#define TX_QUEUE       0u
#define TX_RATE        3u /* 11 Mbps CCK; no OFDM PLCP word */
/* Descriptor words for a protected (CCMP) data frame. The engine picks the TX key
   by the slot index carried in DC8 bits 16-23 — unlike RX, which address-matches
   A2 — so the pairwise slot must be named here, or the frame encrypts against an
   empty slot and the TX pipeline stalls with no completion. */
#define TX_FMT_BITS    0x01600000u /* format: crypto-route + non-aggregated data class */
#define TX_KEYSLOT     6u /* pairwise (PTK) key-table slot */
#define TX_DD0         0x002c0000u /* duration */
#define TX_DD4         0x003ff000u /* frame lifetime; zero can age the frame out before TX */

extern unsigned char wifi_mac_addr[6];
extern volatile unsigned int wifi_fiq_tx_count;

static struct lldesc tx_desc __attribute__((aligned(4)));
static unsigned char tx_buf[1600] __attribute__((aligned(4)));

volatile unsigned int wifi_tx_done_count;

int wifi_tx_frame(const unsigned char *frame, unsigned int len) {
    if (len > sizeof(tx_buf))
        return -1;
    for (unsigned int i = 0; i < len; i++)
        tx_buf[i] = frame[i];

    unsigned int air = len + 4;
    unsigned int B = TXQ(TX_QUEUE);

    /* Protected frames carry the Protected bit + CCMP header + plaintext; the engine
       encrypts and appends the MIC, and needs the pairwise key slot in the descriptor
       (see TX_KEYSLOT). Plaintext frames (EAPOL/probe) use the plain descriptor. */
    unsigned int protectd = (len >= 2 && (tx_buf[1] & 0x40));
    unsigned int dc4, dc8, dd0, dd4;
    if (protectd) {
        dc4 = TX_FMT_BITS;
        dc8 = (air & 0xfffu) | (TX_RATE << 12) | (TX_KEYSLOT << 16);
        dd0 = TX_DD0;
        dd4 = TX_DD4;
    } else {
        dc4 = 0x00400000u;
        dc8 = air | (TX_RATE << 12);
        dd0 = 0;
        dd4 = 0;
    }

    tx_desc.size = air;
    tx_desc.length = air;
    tx_desc.offset = 0;
    tx_desc.sosf = 0;
    tx_desc.eof = 1;
    tx_desc.owner = 1;
    tx_desc.buf_ptr = tx_buf;
    tx_desc.next = 0;

    unsigned int daddr = (unsigned int) &tx_desc & 0x3ffffu;

    unsigned int prev_tx_count = wifi_fiq_tx_count;

    __asm__ volatile("memw");
    WRITE_REG(B + 0x08, dc8);
    WRITE_REG(B + 0x10, dd0);
    WRITE_REG(B + 0x14, dd4);
    WRITE_REG(B + 0x00, (air << 12) & 0x3ff000u);
    WRITE_REG(B + 0x04, daddr | dc4);
    __asm__ volatile("memw");
    WRITE_REG(B + 0x04, READ_REG(B + 0x04) | TX_GO_BITS);

    /* Bounded spin: also the settle that keeps the next frame from reusing the
       descriptor before the DMA has read it. */
    int rc = 0;
    for (int t = 0; t < 200000; t++) {
        if (wifi_fiq_tx_count != prev_tx_count) {
            wifi_tx_done_count++;
            rc = 1;
            break;
        }
    }
    return rc;
}

extern unsigned char ap_bssid[6];

static unsigned int build_probe_req(unsigned char *b) {
    unsigned int n = 0;
    b[n++] = 0x40; b[n++] = 0x00;
    b[n++] = 0x00; b[n++] = 0x00;
    for (int i = 0; i < 6; i++) b[n++] = ap_bssid[i];
    for (int i = 0; i < 6; i++) b[n++] = wifi_mac_addr[i];
    for (int i = 0; i < 6; i++) b[n++] = ap_bssid[i];
    b[n++] = 0x00; b[n++] = 0x00;
    b[n++] = 0x00; b[n++] = 0x00;
    b[n++] = 0x01; b[n++] = 0x04;
    b[n++] = 0x82; b[n++] = 0x84; b[n++] = 0x8b; b[n++] = 0x96;
    return n;
}

int wifi_tx_probe_req(void) {
    unsigned char f[64];
    unsigned int n = build_probe_req(f);
    return wifi_tx_frame(f, n);
}
