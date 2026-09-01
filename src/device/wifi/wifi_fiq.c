#include "def.h"
#include "reg_util.h"

#define MAC_INT_EVENT  0x3ff20c20
#define MAC_INT_CLEAR  0x3ff20c24

#define FIQ_TX_DONE   (1u << 19)
#define FIQ_MAC_TIMER (1u << 27)

extern unsigned int wifi_rx_nmi_drain(void);
extern int wifi_rx_cond;
extern void cond_signal_nmi(int c);

volatile unsigned int wifi_fiq_tx_count;

// unconsumed rx descriptors hold the event line, so the drain has to run before the status clear
IRAM_ATTR void wifi_fiq_dispatch(void) {
    unsigned int status = READ_REG(MAC_INT_EVENT);
    (void)READ_REG(0x3ff20c84);

    if (status & FIQ_TX_DONE)
        wifi_fiq_tx_count++;

    if (wifi_rx_nmi_drain())
        cond_signal_nmi(wifi_rx_cond);

    WRITE_REG(MAC_INT_CLEAR, status);
}
