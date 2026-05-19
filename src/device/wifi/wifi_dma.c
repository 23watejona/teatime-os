#include "reg_util.h"
#include "wifi_dma.h"

extern char *alloc(unsigned int);

#define RX_RING_N 8
#define RX_BUF_SIZE 1600

static struct lldesc *rx_ring;

void init_wifi_dma(void) {
    rx_ring = (struct lldesc *) alloc(sizeof(struct lldesc) * RX_RING_N);
    for (int i = 0; i < RX_RING_N; i++) {
        rx_ring[i].size = RX_BUF_SIZE;
        rx_ring[i].length = RX_BUF_SIZE;
        rx_ring[i].offset = 0;
        rx_ring[i].sosf = 0;
        rx_ring[i].eof = 0;
        rx_ring[i].owner = 1;
        rx_ring[i].buf_ptr = alloc(RX_BUF_SIZE);
        rx_ring[i].next = &rx_ring[(i + 1) % RX_RING_N];
    }

    WRITE_REG(0x3ff20080, (unsigned int) &rx_ring[0]);
    WRITE_REG(0x3ff2007c, (unsigned int) &rx_ring[RX_RING_N - 1]);
    WRITE_REG(0x3ff20088, 0);
    WRITE_REG(0x3ff20084, 0);
    WRITE_REG(0x3ff2000c, 0);
    WRITE_REG(0x3ff20000, READ_REG(0x3ff20000) & 0xffffff00);
}

void lldesc_init_tx(struct lldesc *d, void *buf, unsigned int len) {
    d->size = len;
    d->length = len;
    d->offset = 0;
    d->sosf = 0;
    d->eof = 1;
    d->owner = 1;
    d->buf_ptr = buf;
    d->next = 0;
}
