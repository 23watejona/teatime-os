#include "timer.h"
#include "uart.h"
#include "wifi_tx.h"

/* driven from the RX poll loop, once per iteration */

extern int wifi_locked_channel;
extern volatile unsigned int wifi_rx_directed_count;
extern volatile unsigned int wifi_rx_frames;
extern volatile unsigned int wifi_tx_last_ctrl;

#define AP_CHANNEL     6
#define PROBE_PERIOD   40000000u   /* ~500 ms at 80 MHz */

void wifi_station_tick(void) {
    static int inited;
    static unsigned int last;
    static unsigned int sent;

    if (!inited) {
        wifi_locked_channel = AP_CHANNEL;
        last = ccount();
        inited = 1;
        return;
    }

    if (ccount() - last < PROBE_PERIOD)
        return;
    last = ccount();

    int r = wifi_tx_probe_req();
    sent++;
    kprintf_uart("probe #%u -> %s  ctrl=%x  tx_done=%u  dir=%u  rx=%u\n",
                 sent, r == 1 ? "TXDONE" : (r == 0 ? "timeout" : "toolong"),
                 wifi_tx_last_ctrl, wifi_tx_done_count, wifi_rx_directed_count, wifi_rx_frames);
}
