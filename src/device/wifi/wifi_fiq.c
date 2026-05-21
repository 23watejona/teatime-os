#include "def.h"
#include "reg_util.h"
#include "uart.h"
#include "wdt.h"

#define MAC_INT_EVENT  0x3ff20c20
#define MAC_INT_CLEAR  0x3ff20c24

/* DPORT NMI enable; the WiFi-MAC NMI auto-disables on entry, re-armed by writing 1. */
#define NMI_INT_ENABLE_REG 0x3ff00000

#define FIQ_RX_DONE (1u << 26)
#define FIQ_TX_DONE (1u << 19)

volatile unsigned int wifi_fiq_tx_count;
volatile unsigned int wifi_rx_pending;
volatile unsigned int wifi_nmi_count;

IRAM_ATTR void wifi_fiq_dispatch(void) {
    wifi_nmi_count++;

    /* NMI-storm detector: >2000 back-to-back NMIs <100us apart -> reboot. */
    static unsigned int last_cc;
    static unsigned int streak;
    unsigned int cc;
    asm("rsr.ccount %0" : "=r"(cc));
    if (cc - last_cc < 8000)
        streak++;
    else
        streak = 0;
    last_cc = cc;
    if (streak > 2000) {
        kprintf_uart("\n!!! NMI STORM nmi=%d !!! rebooting\n", wifi_nmi_count);
        system_reboot();
    }

    unsigned int status = READ_REG(MAC_INT_EVENT);

    if (status & FIQ_RX_DONE)
        wifi_rx_pending = 1;
    if (status & FIQ_TX_DONE)
        wifi_fiq_tx_count++;

    WRITE_REG(MAC_INT_CLEAR, status);

    WRITE_REG(NMI_INT_ENABLE_REG, 1);
}
