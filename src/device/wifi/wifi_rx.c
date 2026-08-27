#include "wifi_sta.h"
#include "timer.h"
#include "def.h"
#include "reg_util.h"
#include "wifi_dma.h"
#include "wifi_sta.h"
#include "proc.h"

// RX_CURRENT is read-only and shows where the dma is, so the software head goes into RX_HEAD only when the dma isn't on it
#define MAC_DMA_RX_HEAD     0x3ff20008
#define MAC_DMA_RX_CURRENT  0x3ff2001c

/* Channel dwell: ~400 ms at 80 MHz, a few beacon intervals. */
#define CHAN_DWELL_CYCLES   32000000u

#define STAGE_SLOTS 8

extern void wifi_set_channel(unsigned int ch);
extern void wifi_ap_observe(volatile unsigned char *buf, unsigned int buflen);
extern void wifi_wpa_input(volatile unsigned char *buf, unsigned int len);
extern void wifi_wpa_eapol(unsigned char *llc, unsigned int len);
extern int wifi_ccmp_rx(volatile unsigned char *buf, unsigned int len, unsigned char *out);
extern void net_recv(unsigned char *llc, unsigned int len);
extern void net_tick(void);
extern int disable(void);
extern void enable(int mask);
extern void sched(void);
extern volatile unsigned int wifi_rx_pending;

int wifi_rx_servicer_pid = -1;
volatile unsigned int wifi_tick_pending;
volatile unsigned int wifi_rx_dropped;

// zero means keep hopping channels 1..13
int wifi_locked_channel;

// nmi-to-servicer ring with free-running indices: empty is head == tail, full is head - tail == STAGE_SLOTS and the producer drops; the producer fills the slot, memw, then advances head so the consumer never sees a half-written slot
struct rx_stage {
    unsigned char buf[RX_BUF_LEN];
    unsigned short len;
} __attribute__((aligned(4)));

static struct rx_stage stage[STAGE_SLOTS];
static volatile unsigned int stage_head;
static volatile unsigned int stage_tail;

static volatile struct lldesc *rx_cursor;

IRAM_ATTR static void rx_refill(volatile struct lldesc *d) {
    d->length = d->size;
    d->offset = 0;
    d->sosf = 0;
    d->eof = 0;
    d->owner = 1;
}

// this dma never flips owner back, so completion is the eof bit and owner stays 1
IRAM_ATTR unsigned int wifi_rx_nmi_drain(void) {
    if (!rx_cursor) {
        if (!rx_ring)
            return 0;
        rx_cursor = rx_ring;
    }
    unsigned int consumed = 0;
    while (rx_cursor->eof) {
        unsigned int len = rx_cursor->length;
        // a corrupt length would copy past the stage buffer, so clamp it
        if (len > RX_BUF_LEN)
            len = RX_BUF_LEN;
        if (stage_head - stage_tail < STAGE_SLOTS) {
            struct rx_stage *s = &stage[stage_head & (STAGE_SLOTS - 1)];
            const unsigned char *src =
                (const unsigned char *) rx_cursor->buf_ptr + rx_cursor->offset;
            // memcpy lives in flash and the nmi can only execute iram, so copy by hand
            unsigned int i = 0;
            if (((unsigned int) src & 3) == 0)
                for (; i + 4 <= len; i += 4)
                    *(unsigned int *) (s->buf + i) = *(const unsigned int *) (src + i);
            for (; i < len; i++)
                s->buf[i] = src[i];
            s->len = (unsigned short) len;
            asm volatile("memw");
            stage_head = stage_head + 1;
        } else {
            wifi_rx_dropped++;
        }
        rx_refill(rx_cursor);
        rx_cursor = rx_cursor->next;
        consumed++;
    }
    if (consumed && READ_REG(MAC_DMA_RX_CURRENT) != (unsigned int) rx_cursor)
        WRITE_REG(MAC_DMA_RX_HEAD, (unsigned int) rx_cursor);
    return consumed;
}

static void rx_dump(unsigned char *p, unsigned int len) {
    wifi_ap_observe(p, len);
    wifi_sta_input(p, len);
    wifi_wpa_input(p, len);

    static unsigned char llc[2048] __attribute__((aligned(4)));
    int nl = wifi_ccmp_rx(p, len, llc);
    if (nl > 0) {
        if (nl >= 8 && llc[6] == 0x88 && llc[7] == 0x8e)
            wifi_wpa_eapol(llc, (unsigned int) nl);
        else
            net_recv(llc, (unsigned int) nl);
    }
}

/* Wakes may be spurious. Clear the wake flags before consuming, and re-check
   them under the mask before parking. */
void wifi_rx_servicer(void) {
    unsigned int ch = 1;
    int locked_now = 0;
    unsigned int last = ccount();
    while (1) {
        int m = disable();
        wifi_rx_pending = 0;
        wifi_tick_pending = 0;
        enable(m);

        while (stage_tail != stage_head) {
            struct rx_stage *s = &stage[stage_tail & (STAGE_SLOTS - 1)];
            rx_dump(s->buf, s->len);
            stage_tail = stage_tail + 1;
        }
        wifi_station_tick();
        net_tick();

        if (wifi_locked_channel) {
            if (locked_now != wifi_locked_channel) {
                wifi_set_channel(wifi_locked_channel);
                locked_now = wifi_locked_channel;
            }
        } else {
            locked_now = 0;
            if (ccount() - last >= CHAN_DWELL_CYCLES) {
                ch = (ch >= 13) ? 1 : ch + 1;
                wifi_set_channel(ch);
                last = ccount();
            }
        }

        m = disable();
        if (!wifi_rx_pending && !wifi_tick_pending) {
            proctab[wifi_rx_servicer_pid].status = PROC_IO_WAIT;
            sched();
        }
        enable(m);
    }
}
