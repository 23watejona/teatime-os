#ifndef WIFI_DMA_H
#define WIFI_DMA_H

struct lldesc {
    unsigned int size   : 12;
    unsigned int length : 12;
    unsigned int offset : 5;
    unsigned int sosf   : 1;
    unsigned int eof    : 1;
    unsigned int owner  : 1;
    void *buf_ptr;
    struct lldesc *next;
};
_Static_assert(sizeof(struct lldesc) == 12, "lldesc must be 12 bytes");

void init_wifi_dma(void);
void lldesc_init_tx(struct lldesc *d, void *buf, unsigned int len);

#endif
