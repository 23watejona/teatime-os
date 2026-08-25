#include "reg_util.h"
#include "wait.h"
#include "uart.h"
#include "wifi_dma.h"
#include "wifi_tx.h"
#include "ap_secrets.h"

// tx is a per-queue mmio block rather than a descriptor ring: the length, plcp and descriptor words are armed first and the go bits set last, so the queue never launches a half-written frame; completion is TX_DONE_BIT in MAC_INT_EVENT

#define TXQ(q)         (0x3ff20dc0u - 0x18u * (q))
#define MAC_INT_EVENT  0x3ff20c20u
#define MAC_INT_CLEAR  0x3ff20c24u
#define TX_DONE_BIT    0x00080000u
#define TX_GO_BITS     0xc0000000u

#define TX_QUEUE       0u
#define TX_RATE        0u            /* 1 Mbps CCK; no OFDM PLCP word */
/* bit24 = PLCP format, bit22 = 11b/CCK */
#define TX_FMT_BITS    0x01400000u

extern unsigned char wifi_mac_addr[6];

static struct lldesc tx_desc __attribute__((aligned(4)));
static unsigned char tx_buf[1600] __attribute__((aligned(4)));

volatile unsigned int wifi_tx_done_count;
volatile unsigned int wifi_tx_last_ctrl;

int wifi_tx_frame(const unsigned char *frame, unsigned int len) {
    if (len > sizeof(tx_buf))
        return -1;
    for (unsigned int i = 0; i < len; i++)
        tx_buf[i] = frame[i];

    unsigned int air = len + 4;             /* + FCS */
    unsigned int B = TXQ(TX_QUEUE);

    /* owner=1 or the engine raises TX_DONE_BIT without ever DMAing the frame */
    tx_desc.size = air;
    tx_desc.length = air;
    tx_desc.offset = 0;
    tx_desc.sosf = 0;
    tx_desc.eof = 1;
    tx_desc.owner = 1;
    tx_desc.buf_ptr = tx_buf;
    tx_desc.next = 0;

    unsigned int daddr = (unsigned int) &tx_desc & 0x3ffffu;

    WRITE_REG(MAC_INT_CLEAR, TX_DONE_BIT);

    __asm__ volatile("memw");
    WRITE_REG(B + 0x08, air | (TX_RATE << 12));
    WRITE_REG(B + 0x10, 0);
    WRITE_REG(B + 0x14, 0);
    WRITE_REG(B + 0x00, (air << 12) & 0x3ff000u);
    WRITE_REG(B + 0x04, daddr | TX_FMT_BITS);
    __asm__ volatile("memw");
    WRITE_REG(B + 0x04, READ_REG(B + 0x04) | TX_GO_BITS);

    int rc = 0;
    for (int t = 0; t < 200000; t++) {
        if (READ_REG(MAC_INT_EVENT) & TX_DONE_BIT) {
            WRITE_REG(MAC_INT_CLEAR, TX_DONE_BIT);
            wifi_tx_done_count++;
            rc = 1;
            break;
        }
    }
    wifi_tx_last_ctrl = READ_REG(B + 0x04);
    return rc;
}

/* strong ch6 AP, used only to elicit a probe response and confirm we radiate */
static const unsigned char probe_target[6] = { AP_BSSID_BYTES };

static unsigned int build_probe_req(unsigned char *b) {
    unsigned int n = 0;
    b[n++] = 0x40; b[n++] = 0x00;
    b[n++] = 0x00; b[n++] = 0x00;
    for (int i = 0; i < 6; i++) b[n++] = probe_target[i];   /* addr1 DA */
    for (int i = 0; i < 6; i++) b[n++] = wifi_mac_addr[i];  /* addr2 SA */
    for (int i = 0; i < 6; i++) b[n++] = probe_target[i];   /* addr3 BSSID */
    b[n++] = 0x00; b[n++] = 0x00;
    b[n++] = 0x00; b[n++] = 0x00;                            /* SSID IE, wildcard */
    b[n++] = 0x01; b[n++] = 0x04;
    b[n++] = 0x82; b[n++] = 0x84; b[n++] = 0x8b; b[n++] = 0x96;
    return n;
}

int wifi_tx_probe_req(void) {
    unsigned char f[64];
    unsigned int n = build_probe_req(f);
    return wifi_tx_frame(f, n);
}
