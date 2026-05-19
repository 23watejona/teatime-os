#include "def.h"
#include "reg_util.h"

#define MAC_INT_STATUS 0x3ff20c18
#define MAC_INT_EVENT  0x3ff20c20
#define MAC_INT_CLEAR  0x3ff20c24

#define FIQ_RX_DONE (1u << 26)
#define FIQ_TX_DONE (1u << 19)

volatile unsigned int wifi_fiq_tx_count;

IRAM_ATTR void wifi_fiq_dispatch(void) {
    unsigned int status = READ_REG(MAC_INT_EVENT);

    if (status & FIQ_TX_DONE)
        wifi_fiq_tx_count++;

    WRITE_REG(MAC_INT_CLEAR, status);
    WRITE_REG(MAC_INT_STATUS, status);
}
