#include "def.h"
#include "reg_util.h"
#include "uart.h"
#include "wdt.h"

#define MAC_INT_EVENT  0x3ff20c20
#define MAC_INT_CLEAR  0x3ff20c24

/* NMI auto-disables on entry; re-arm by writing 1. */
#define NMI_INT_ENABLE_REG 0x3ff00000

/* bit 26 also matches (status & 0xc) but is a MAC-timer slot, not RX. */
#define FIQ_RX_MASK   0x0000000c
#define FIQ_RX_DONE   0x00000008
#define FIQ_TX_DONE   (1u << 19)
#define FIQ_MAC_TIMER (1u << 27) /* acked with its own fixed mask below */
#define FIQ_FATAL     ((1u << 20) | (1u << 28))

volatile unsigned int wifi_fiq_tx_count;
volatile unsigned int wifi_rx_pending;
volatile unsigned int wifi_nmi_count;

IRAM_ATTR void wifi_fiq_dispatch(void) {
    wifi_nmi_count++;

    /* NMI-storm guard: reboot after >2000 NMIs <100us apart. */
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

    unsigned int interest = READ_REG(0x3ff20c18);
    unsigned int status = READ_REG(MAC_INT_EVENT);
    (void)READ_REG(0x3ff20c84);
    WRITE_REG(0x3ff20c18, 0);

    if (status & FIQ_MAC_TIMER) {
        WRITE_REG(MAC_INT_CLEAR, FIQ_MAC_TIMER);
        status &= ~FIQ_MAC_TIMER;
    }

    if (status & FIQ_FATAL) {
        kprintf_uart("\n!!! WDEV FATAL status=%x !!! rebooting\n", status);
        system_reboot();
    }

    if ((status & FIQ_RX_MASK) == FIQ_RX_DONE)
        wifi_rx_pending = 1;
    if (status & FIQ_TX_DONE)
        wifi_fiq_tx_count++;

    WRITE_REG(NMI_INT_ENABLE_REG, 0);
    while (READ_REG(NMI_INT_ENABLE_REG) != 0)
        WRITE_REG(NMI_INT_ENABLE_REG, 0);

    WRITE_REG(MAC_INT_CLEAR, status);
    WRITE_REG(0x3ff20c18, interest);

    WRITE_REG(NMI_INT_ENABLE_REG,
              (READ_REG(NMI_INT_ENABLE_REG) & 0xffffffe0) | 1);
}
