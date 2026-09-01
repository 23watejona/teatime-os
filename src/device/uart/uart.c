#include "stdarg.h"
#include "def.h"
#include "uart.h"
#include "dev.h"
#include "proc.h"
#include "intr.h"

char *itoau(int, char *, int);
char *itoa(int, char *, int);

#define RX_RING_SIZE 256
#define RX_FULL_THRESHOLD 64
#define RX_TIMEOUT_THRESHOLD 2
#define TX_EMPTY_THRESHOLD 64

static unsigned char rx_ring[RX_RING_SIZE];
static volatile unsigned int rx_head;
static volatile unsigned int rx_tail;
static volatile unsigned int rx_dropped;
static int rx_mutex;
static int tx_mutex;
static struct dev *uart_dev;

inline __attribute__((always_inline)) unsigned int uart0_tx_fifo_size() {
    return uart0.status.tx_fifo_count;
}

inline __attribute__((always_inline)) unsigned int uart0_tx_fifo_full() {
    return uart0_tx_fifo_size() >= TX_FIFO_SIZE;
}

void uart0_flush() {
    while (uart0_tx_fifo_size());
}

void kputc_uart(int c) {
    if (c == '\n') {
        while(uart0_tx_fifo_full());
        uart0.fifo.rw = '\r';
    }
    while(uart0_tx_fifo_full());
    uart0.fifo.rw = c;
}

void kprintf_uart(char *f, ...) {
    va_list args;
    va_start(args, f);
    while ((unsigned int)*f != (unsigned int)'\0') {
        if (*f != '%') {
            kputc_uart(*f++);
        } else {
            switch (*(++f)) {
                case 'd':
                    {
                        int tmp_d = va_arg(args, int);
                        char buf[12] = {0};
                        kprintf_uart(itoa(tmp_d, buf, 10));
                        ++f;
                    }
                    break;
                case 'u':
                    {
                        unsigned int tmp_d = va_arg(args, unsigned int);
                        char buf[12] = {0};
                        kprintf_uart(itoau(tmp_d, buf, 10));
                        ++f;
                    }
                    break;
                case 'x':
                    {
                        unsigned int tmp_d = va_arg(args, unsigned int);
                        char buf[12] = {0};
                        kprintf_uart("0x");
                        kprintf_uart(itoau(tmp_d, buf, 16));
                        ++f;
                    }
                    break;
                case 's':
                    {
                        char *str = va_arg(args, char *);
                        kprintf_uart(str);
                        ++f;
                    }
                    break;
                default:
                    kputc_uart(*f++);
            }
        }
    }
    uart0_flush();
}

static void uart_intr(void) {
    int tx_drained = uart0.int_status.txfifo_empty;
    // clear before draining, so a byte that lands after the drain raises its own timeout event
    uart0.int_clear.rxfifo_full = 1;
    uart0.int_clear.rxfifo_timeout = 1;
    uart0.int_clear.txfifo_empty = 1;
    if (tx_drained) { // the empty condition holds until the writer refills, so mask it here and let uart_write re-arm
        uart0.int_enable.txfifo_empty = 0;
        cond_signal_isr(uart_dev->cond);
    }
    int received = uart0.status.rx_fifo_count != 0;
    while (uart0.status.rx_fifo_count) {
        unsigned char c = uart0.fifo.rw;
        if (rx_head - rx_tail < RX_RING_SIZE) {
            rx_ring[rx_head & (RX_RING_SIZE - 1)] = c;
            rx_head = rx_head + 1;
        } else {
            rx_dropped = rx_dropped + 1;
        }
    }
    if (received)
        cond_signal_isr(uart_dev->cond);
}

static int uart_read(struct dev *d, void *buf, unsigned int n) {
    unsigned char *out = buf;
    unsigned int got = 0;
    mutex_lock(rx_mutex);
    while (rx_head == rx_tail)
        cond_wait(uart_dev->cond, rx_mutex);
    while (got < n && rx_head != rx_tail) {
        out[got] = rx_ring[rx_tail & (RX_RING_SIZE - 1)];
        rx_tail = rx_tail + 1;
        got++;
    }
    mutex_unlock(rx_mutex);
    return got;
}

static int uart_write(struct dev *d, const void *buf, unsigned int n) {
    const unsigned char *in = buf;
    unsigned int sent = 0;
    mutex_lock(tx_mutex);
    while (sent < n) {
        while (sent < n && !uart0_tx_fifo_full()) {
            uart0.fifo.rw = in[sent];
            sent++;
        }
        if (sent < n) {
            uart0.int_enable.txfifo_empty = 1;
            cond_wait(uart_dev->cond, tx_mutex);
        }
    }
    mutex_unlock(tx_mutex);
    return n;
}

static const struct dev_ops uart_ops = {
    .read = uart_read,
    .write = uart_write,
};

void uart_init(void) {
    rx_mutex = mutex_create();
    tx_mutex = mutex_create();
    uart0.int_enable.rxfifo_full = 0;
    uart0.int_enable.txfifo_empty = 0;
    uart0.int_enable.rxfifo_timeout = 0;
    uart0.conf1.rxfifo_full_threshold = RX_FULL_THRESHOLD;
    uart0.conf1.txfifo_empty_threshold = TX_EMPTY_THRESHOLD;
    uart0.conf1.rx_flow_enable = 0;
    uart0.conf1.rx_timeout_threshold = RX_TIMEOUT_THRESHOLD;
    uart0.conf1.rx_timeout_enable = 1;
    while (uart0.status.rx_fifo_count)
        (void)uart0.fifo.rw;
    uart0.int_clear.rxfifo_full = 1;
    uart0.int_clear.rxfifo_timeout = 1;
    uart0.int_clear.txfifo_empty = 1;
    l1_interrupt_handlers[INUM_UART] = uart_intr;
    uart0.int_enable.rxfifo_full = 1;
    uart0.int_enable.rxfifo_timeout = 1;
    intr_unmask(1u << INUM_UART);
    uart_dev = dev_register("uart0", &uart_ops, NULL);
}
