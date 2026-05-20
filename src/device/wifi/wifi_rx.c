#include "def.h"
#include "uart.h"
#include "reg_util.h"
#include "proc.h"
#include "proc_queue.h"
#include "wifi_dma.h"

#define MAC_DMA_RX_TAIL 0x3ff2007c
#define RX_BUF_SIZE 1600

extern void make_avail(int pid);
extern void sched(void);
extern int disable(void);
extern void enable(int mask);
extern volatile unsigned int wifi_rx_pending;

int wifi_rx_servicer_pid = -1;

static volatile struct lldesc *rx_cursor;

static void rx_dump(volatile struct lldesc *d) {
    volatile unsigned char *p = (volatile unsigned char *) d->buf_ptr + d->offset;
    unsigned int len = d->length;
    unsigned int fc = p[0] | (p[1] << 8);
    kprintf_uart("rx: len=%d fc=%x type=%d subtype=%d\n",
                 len, fc, (fc >> 2) & 3, (fc >> 4) & 0xf);
    for (unsigned int i = 0; i < len; i++) {
        kprintf_uart("%x ", p[i]);
        if ((i & 0xf) == 0xf)
            kputc_uart('\n');
    }
    kputc_uart('\n');
}

static void rx_refill(volatile struct lldesc *d) {
    d->size = RX_BUF_SIZE;
    d->length = RX_BUF_SIZE;
    d->offset = 0;
    d->sosf = 0;
    d->eof = 0;
    d->owner = 1;
    WRITE_REG(MAC_DMA_RX_TAIL, (unsigned int) d);
}

static int rx_drain(void) {
    int n = 0;
    while (rx_cursor->owner == 0) {
        rx_dump(rx_cursor);
        rx_refill(rx_cursor);
        rx_cursor = rx_cursor->next;
        n++;
    }
    return n;
}

static void rx_block(void) {
    int mask = disable();
    proc_remove(wifi_rx_servicer_pid);
    proctab[wifi_rx_servicer_pid].status = PROC_IO_WAIT;
    sched();
    enable(mask);
}

void wifi_rx_servicer(void) {
    rx_cursor = rx_ring;
    while (1) {
        wifi_rx_pending = 0;
        if (rx_drain() == 0)
            rx_block();
    }
}
