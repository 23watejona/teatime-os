#include "def.h"
#include "reg_util.h"
#include "wifi_regs.h"

extern unsigned int wifi_rx_nmi_drain(void);
extern int wifi_rx_cond;
extern void wifi_tx_dma_done(void);
extern void cond_signal_nmi(int c);

volatile unsigned int wifi_fiq_tx_count;

// unconsumed rx descriptors hold the event line, so the drain has to run before the status clear
IRAM_ATTR void wifi_fiq_dispatch(void) {
    unsigned int status = READ_REG(MAC_INT_EVENT);
    (void)READ_REG(MAC_TX_STATUS);

    if (status & MAC_INT_TX_DONE) {
        wifi_fiq_tx_count++;
        wifi_tx_dma_done();
    }

    if (wifi_rx_nmi_drain())
        cond_signal_nmi(wifi_rx_cond);

    WRITE_REG(MAC_INT_CLEAR, status);
}
