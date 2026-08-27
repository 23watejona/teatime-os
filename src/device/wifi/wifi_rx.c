#include "wifi_sta.h"
#include "timer.h"
#include "def.h"
#include "reg_util.h"
#include "wifi_dma.h"

// RX_CURRENT is read-only and shows where the dma is, so the software head goes into RX_HEAD only when the dma isn't on it
#define MAC_DMA_RX_HEAD     0x3ff20008
#define MAC_DMA_RX_CURRENT  0x3ff2001c

/* Channel dwell: ~400 ms at 80 MHz, a few beacon intervals. */
#define CHAN_DWELL_CYCLES   32000000u

extern void wifi_set_channel(unsigned int ch);
extern void wifi_ap_observe(volatile unsigned char *buf, unsigned int buflen);
extern void wifi_wpa_input(volatile unsigned char *buf, unsigned int len);
extern void wifi_wpa_eapol(unsigned char *llc, unsigned int len);
extern int wifi_ccmp_rx(volatile unsigned char *buf, unsigned int len, unsigned char *out);
extern void net_recv(unsigned char *llc, unsigned int len);
extern void net_tick(void);
extern void wifi_rx_poll_done(void);

int wifi_rx_servicer_pid = -1;

// zero means keep hopping channels 1..13
int wifi_locked_channel;

static volatile struct lldesc *rx_cursor;

static void rx_dump(volatile struct lldesc *d) {
    volatile unsigned char *p = (volatile unsigned char *) d->buf_ptr + d->offset;
    wifi_ap_observe(p, d->length);
    wifi_sta_input(p, d->length);
    wifi_wpa_input(p, d->length);

    static unsigned char llc[2048] __attribute__((aligned(4)));
    int nl = wifi_ccmp_rx(p, d->length, llc);
    if (nl > 0) {
        if (nl >= 8 && llc[6] == 0x88 && llc[7] == 0x8e)
            wifi_wpa_eapol(llc, (unsigned int) nl);
        else
            net_recv(llc, (unsigned int) nl);
    }
}

static void rx_refill(volatile struct lldesc *d) {
    d->length = d->size;
    d->offset = 0;
    d->sosf = 0;
    d->eof = 0;
    d->owner = 1;
}

/* This DMA signals a completed frame with the descriptor's EOF bit (and length),
 * leaving owner=1 — not by clearing owner as a classic lldesc ring would. Drain on
 * EOF, refilling each consumed descriptor and republishing the head. */
static int rx_drain(void) {
    int n = 0;
    while (rx_cursor->eof) {
        rx_dump(rx_cursor);
        rx_refill(rx_cursor);
        rx_cursor = rx_cursor->next;
        n++;
    }
    if (n && READ_REG(MAC_DMA_RX_CURRENT) != (unsigned int) rx_cursor)
        WRITE_REG(MAC_DMA_RX_HEAD, (unsigned int) rx_cursor);
    return n;
}

/* Poll the RX ring and hop channels 1..13. The interrupt-driven wake path does
 * not work yet (the NMI frame handler faults on entry), so this busy-polls; the
 * DMA fills the ring continuously, so polling loses no frames. */
void wifi_rx_servicer(void) {
    rx_cursor = rx_ring;
    unsigned int ch = 1;
    int locked_now = 0;
    unsigned int last = ccount();
    while (1) {
        rx_drain();
        wifi_rx_poll_done();
        wifi_station_tick();
        net_tick();
        if (wifi_locked_channel) {
            if (locked_now != wifi_locked_channel) {
                wifi_set_channel(wifi_locked_channel);
                locked_now = wifi_locked_channel;
            }
            continue;
        }
        locked_now = 0;
        if (ccount() - last >= CHAN_DWELL_CYCLES) {
            ch = (ch >= 13) ? 1 : ch + 1;
            wifi_set_channel(ch);
            last = ccount();
        }
    }
}
