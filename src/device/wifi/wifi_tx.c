#include "def.h"
#include "reg_util.h"
#include "wait.h"
#include "uart.h"
#include "proc.h"
#include "timer.h"
#include "wifi_dma.h"
#include "wifi_tx.h"

// tx is a per-queue mmio block rather than a descriptor ring: the length, plcp and descriptor words are armed first and the go bits set last, so the queue never launches a half-written frame; completion is TX_DONE_BIT in MAC_INT_EVENT

#define TXQ(q)         (0x3ff20dc0u - 0x18u * (q))
#define MAC_INT_EVENT  0x3ff20c20u
#define MAC_INT_CLEAR  0x3ff20c24u
#define TX_DONE_BIT    0x00080000u
#define TX_GO_BITS     0xc0000000u

#define TX_QUEUE       0u
#define TX_RATE        3u // 11 mbps cck, so no ofdm plcp word is needed
// the tx engine picks its key by the slot index in dc8 bits 16-23, not by address like rx, so the pairwise slot must be named or the frame encrypts against an empty slot and tx stalls with no completion
#define TX_FMT_BITS    0x01600000u // crypto-routed, non-aggregated data
#define TX_KEYSLOT     6u // must match the pairwise slot wifi_ccmp_install_keys writes
#define TX_DD0         0x002c0000u // duration
#define TX_DD4         0x003ff000u // frame lifetime; zero can age the frame out before it ever transmits
#define TX_DONE_TICKS  (TICKS_PER_SEC / 50)
#define DESCRIPTOR_TICKS (TICKS_PER_SEC / 10)

extern unsigned char wifi_mac_addr[6];
extern volatile unsigned int wifi_fiq_tx_count;

static struct lldesc tx_desc __attribute__((aligned(4)));
static unsigned char tx_buf[1600] __attribute__((aligned(4)));
// guards descriptor_busy, tx_desc, tx_buf, tx_seq and the packet number
static int descriptor_mutex;
static int descriptor_busy; // set while tx_desc and tx_buf belong to a launched frame
static int descriptor_cond;

// guards the queue registers and the tx-done count
static int dma_mutex;
static int dma_cond;

static unsigned int tx_seq;
static unsigned int pn_lo = 1;
static unsigned int pn_hi;

void wifi_tx_init(void) {
    descriptor_mutex = mutex_create();
    descriptor_cond = cond_create();
    dma_mutex = mutex_create();
    dma_cond = cond_create();
}

IRAM_ATTR void wifi_tx_dma_done(void) {
    cond_signal_nmi(dma_cond);
}

int wifi_tx_frame(const unsigned char *frame, unsigned int len) {
    if (len > sizeof(tx_buf))
        return -1;

    unsigned int air = len + 4;
    unsigned int B = TXQ(TX_QUEUE);

    unsigned int protectd = frame[1] & FC_PROTECTED;
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

    mutex_lock(descriptor_mutex);
    while (descriptor_busy) {
        if (cond_timedwait(descriptor_cond, descriptor_mutex, DESCRIPTOR_TICKS) < 0) {
            mutex_unlock(descriptor_mutex);
            return -1;
        }
    }
    descriptor_busy = 1;
    for (unsigned int i = 0; i < len; i++)
        tx_buf[i] = frame[i];

    // the ap drops a repeated sequence number as a retransmit and a lower packet number as a replay, so both are assigned in launch order under the lock
    struct mac_header *mac = (struct mac_header *)tx_buf;
    mac->sequence_control = tx_seq << 4;
    tx_seq = (tx_seq + 1) & 0xfff;
    if (protectd) {
        struct ccmp_header *ccmp = (struct ccmp_header *)(tx_buf + sizeof(struct mac_header));
        ccmp->pn0 = pn_lo;
        ccmp->pn1 = pn_lo >> 8;
        ccmp->reserved = 0;
        ccmp->key_id = CCMP_EXT_IV;
        ccmp->pn2 = pn_lo >> 16;
        ccmp->pn3 = pn_lo >> 24;
        ccmp->pn4 = pn_hi;
        ccmp->pn5 = pn_hi >> 8;
        if (++pn_lo == 0)
            pn_hi++;
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
    mutex_unlock(descriptor_mutex);

    mutex_lock(dma_mutex);
    unsigned int prev_tx_count = wifi_fiq_tx_count;

    __asm__ volatile("memw");
    WRITE_REG(B + 0x08, dc8);
    WRITE_REG(B + 0x10, dd0);
    WRITE_REG(B + 0x14, dd4);
    WRITE_REG(B + 0x00, (air << 12) & 0x3ff000u);
    WRITE_REG(B + 0x04, daddr | dc4);
    __asm__ volatile("memw");
    WRITE_REG(B + 0x04, READ_REG(B + 0x04) | TX_GO_BITS);

    int rc = -1;
    while (wifi_fiq_tx_count == prev_tx_count)
        if (cond_timedwait(dma_cond, dma_mutex, TX_DONE_TICKS) < 0)
            break;
    if (wifi_fiq_tx_count != prev_tx_count)
        rc = 0;
    mutex_unlock(dma_mutex);

    mutex_lock(descriptor_mutex);
    descriptor_busy = 0;
    cond_signal(descriptor_cond);
    mutex_unlock(descriptor_mutex);
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
