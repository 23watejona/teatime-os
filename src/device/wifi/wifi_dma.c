#include "reg_util.h"
#include "wifi_dma.h"
#include "uart.h"

#include "mem.h"

// the rx mac bounds-checks every dma address against two windows and walks the descriptor chain from the base of the first, so descriptors and packet data must come from one contiguous allocation:
//   window 1: [ desc[RX_BUF_NUM] ][ data: RX_BUF_NUM * RX_BUF_LEN ]
//   window 2: [ control descriptor ][ its one-word buffer ]
// the window-1 base points at the descriptor array, not the data

#define ALIGN4(x) ((unsigned int)(((unsigned int)(x) + 3u) & ~3u))

struct lldesc *rx_ring;
struct lldesc *rx_ctrl_desc;
unsigned char *rx_data_base;
unsigned char *rx_data_end;

void init_wifi_dma(void) {
    unsigned int desc_bytes = sizeof(struct lldesc) * RX_BUF_NUM;
    unsigned int total = desc_bytes
                       + RX_BUF_NUM * RX_BUF_LEN
                       + sizeof(struct lldesc) + 4
                       + 4;

    unsigned char *region = (unsigned char *) alloc(total);
    if (!region) {
        kprintf_uart("init_wifi_dma: region alloc failed (%u bytes)\n", total);
        return;
    }

    rx_ring = (struct lldesc *) ALIGN4(region);
    rx_data_base = (unsigned char *) rx_ring + desc_bytes;
    rx_data_end = rx_data_base + RX_BUF_NUM * RX_BUF_LEN;
    rx_ctrl_desc = (struct lldesc *) ALIGN4(rx_data_end);
    unsigned int *ctrl_word = (unsigned int *) (rx_ctrl_desc + 1);

    for (int i = 0; i < RX_BUF_NUM; i++) {
        rx_ring[i].size = RX_BUF_LEN;
        rx_ring[i].length = RX_BUF_LEN;
        rx_ring[i].offset = 0;
        rx_ring[i].sosf = 0;
        rx_ring[i].eof = 0;
        rx_ring[i].owner = 1;
        rx_ring[i].buf_ptr = rx_data_base + i * RX_BUF_LEN;
        rx_ring[i].next = &rx_ring[(i + 1) % RX_BUF_NUM];
    }

    // a null control-chain head wedges the dma engine, so it gets a real one-word descriptor
    *ctrl_word = 0;
    rx_ctrl_desc->size = 4;
    rx_ctrl_desc->length = 0;
    rx_ctrl_desc->offset = 0;
    rx_ctrl_desc->sosf = 0;
    rx_ctrl_desc->eof = 0;
    rx_ctrl_desc->owner = 1;
    rx_ctrl_desc->buf_ptr = ctrl_word;
    rx_ctrl_desc->next = 0;

    kprintf_uart("init_wifi_dma: desc=%x data=%x..%x ctrl=%x\n",
                 rx_ring, rx_data_base, rx_data_end, rx_ctrl_desc);

    WRITE_REG(0x3ff20080, (unsigned int) rx_ring);
    WRITE_REG(0x3ff2007c, (unsigned int) rx_data_end);
    WRITE_REG(0x3ff20088, (unsigned int) rx_ctrl_desc);
    WRITE_REG(0x3ff20084, (unsigned int) (ctrl_word + 1));
    WRITE_REG(0x3ff20000, READ_REG(0x3ff20000) & 0xffffff00);
    WRITE_REG(0x3ff20008, (unsigned int) &rx_ring[0]);
    WRITE_REG(0x3ff2000c, (unsigned int) rx_ctrl_desc);
    WRITE_REG(0x3ff20010, 0);
    WRITE_REG(0x3ff20000, READ_REG(0x3ff20000) & 0xdfffffff);
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
