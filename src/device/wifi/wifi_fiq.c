#include "def.h"
#include "reg_util.h"

#define MAC_INT_EVENT  0x3ff20c20
#define MAC_INT_CLEAR  0x3ff20c24

#define FIQ_RX_MASK   0x0000000c
#define FIQ_TX_DONE   (1u << 19)
#define FIQ_MAC_TIMER (1u << 27)

volatile unsigned int wifi_fiq_tx_count;
volatile unsigned int wifi_rx_pending;
volatile unsigned int wifi_nmi_count;

/* The RX-done event is level-held: the MAC keeps it asserted while a completed
   descriptor is unconsumed. Draining the ring is the RX servicer task's job, so
   the ISR masks the RX bits in the interest register and hands off — otherwise
   the event re-fires the instant the ISR returns and storms the CPU, starving
   the very task that would clear it. wifi_rx_poll_done() re-arms them. */
IRAM_ATTR void wifi_fiq_dispatch(void) {
    wifi_nmi_count++;

    unsigned int interest = READ_REG(0x3ff20c18);
    unsigned int status = READ_REG(MAC_INT_EVENT);
    (void)READ_REG(0x3ff20c84);
    WRITE_REG(0x3ff20c18, 0);

    if (status & FIQ_MAC_TIMER) {
        WRITE_REG(MAC_INT_CLEAR, FIQ_MAC_TIMER);
        status &= ~FIQ_MAC_TIMER;
    }

    if (status & FIQ_RX_MASK) {
        wifi_rx_pending = 1;
        interest &= ~FIQ_RX_MASK; /* masked until the servicer drains */
    }
    if (status & FIQ_TX_DONE)
        wifi_fiq_tx_count++;

    /* The NMI source auto-disables on entry and drive_nmi (intr.s) re-arms it as
       its last write before rfi — so this handler must not touch 0x3ff00000. */
    WRITE_REG(MAC_INT_CLEAR, status);
    WRITE_REG(0x3ff20c18, interest);
}

/* Called from the RX servicer after it drains the ring: the completed
   descriptors are consumed, so re-arm the RX interest bits the ISR masked. */
void wifi_rx_poll_done(void) {
    if (!wifi_rx_pending)
        return;
    wifi_rx_pending = 0;
    WRITE_REG(0x3ff20c18, READ_REG(0x3ff20c18) | FIQ_RX_MASK);
}
