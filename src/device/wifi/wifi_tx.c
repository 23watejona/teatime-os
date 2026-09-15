#include "def.h"
#include "reg_util.h"
#include "wifi_regs.h"
#include "proc.h"
#include "timer.h"
#include "wifi_dma.h"
#include "wifi_frame.h"
#include "wifi_tx.h"
#include "string.h"

// tx is a per-queue mmio block rather than a descriptor ring: the rate, duration, lifetime, length and descriptor words are armed first and the go bits set last, so the queue never launches a half-written frame; completion is MAC_INT_TX_DONE in MAC_INT_EVENT

#define TX_GO_BITS     0xc0000000

#define TX_QUEUE       0
#define TX_RATE        3 // 11 mbps cck, so no ofdm plcp word is needed
// the tx engine picks its key by the slot index in the rate word, not by address like rx, so the pairwise slot must be named or the frame encrypts against an empty slot and tx stalls with no completion
#define TX_FMT_CRYPTO  0x01600000 // crypto-routed, non-aggregated data
#define TX_FMT_PLAIN   0x00400000
#define TX_KEYSLOT     6 // must match the pairwise slot wifi_ccmp_install_keys writes
#define TX_DURATION    0x002c0000
#define TX_LIFETIME    0x003ff000 // zero can age the frame out before it ever transmits
#define TX_DONE_TICKS  (TICKS_PER_SEC / 50)
#define DESCRIPTOR_TICKS (TICKS_PER_SEC / 10)

extern unsigned char wifi_mac_addr[6];
extern volatile unsigned int wifi_fiq_tx_count;

static struct lldesc tx_desc __attribute__((aligned(4)));
static unsigned char tx_buf[MAX_FRAME_LEN] __attribute__((aligned(4)));
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
    unsigned int q = MAC_TXQ(TX_QUEUE);

    unsigned int protected = frame[1] & FC_PROTECTED;
    unsigned int fmt, rate, duration, lifetime;
    if (protected) {
        fmt = TX_FMT_CRYPTO;
        rate = air | (TX_RATE << 12) | (TX_KEYSLOT << 16);
        duration = TX_DURATION;
        lifetime = TX_LIFETIME;
    } else {
        fmt = TX_FMT_PLAIN;
        rate = air | (TX_RATE << 12);
        duration = 0;
        lifetime = 0;
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
    if (protected) {
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

    unsigned int desc_addr = (unsigned int) &tx_desc & 0x3ffff;
    mutex_unlock(descriptor_mutex);

    mutex_lock(dma_mutex);
    unsigned int prev_tx_count = wifi_fiq_tx_count;

    __asm__ volatile("memw");
    WRITE_REG(q + TXQ_RATE, rate);
    WRITE_REG(q + TXQ_DURATION, duration);
    WRITE_REG(q + TXQ_LIFETIME, lifetime);
    WRITE_REG(q + TXQ_LEN, (air << 12) & 0x3ff000);
    WRITE_REG(q + TXQ_DESC, desc_addr | fmt);
    __asm__ volatile("memw");
    WRITE_REG_MASK(q + TXQ_DESC, TX_GO_BITS);

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

static const unsigned char basic_rates[] = {
    RATE_BASIC | RATE_KBPS(1000), RATE_BASIC | RATE_KBPS(2000),
    RATE_BASIC | RATE_KBPS(5500), RATE_BASIC | RATE_KBPS(11000),
};

static unsigned int build_probe_req(unsigned char *b) {
    struct mac_header *mac = (struct mac_header *)b;
    memset(mac, 0, sizeof(*mac));
    mac->frame_control[0] = MGMT_PROBE_REQ << FC_SUBTYPE_SHIFT;
    memcpy(mac->addr1, ap_bssid, 6);
    memcpy(mac->addr2, wifi_mac_addr, 6);
    memcpy(mac->addr3, ap_bssid, 6);
    unsigned int n = sizeof(*mac);

    b[n++] = IE_SSID;
    b[n++] = 0;
    b[n++] = IE_SUPPORTED_RATES;
    b[n++] = sizeof(basic_rates);
    memcpy(b + n, basic_rates, sizeof(basic_rates));
    n += sizeof(basic_rates);
    return n;
}

int wifi_tx_probe_req(void) {
    unsigned char f[64];
    unsigned int n = build_probe_req(f);
    return wifi_tx_frame(f, n);
}
