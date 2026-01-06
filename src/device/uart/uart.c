#include "stdarg.h"
#include "uart.h"

char *itoau(int, char *, int);
char *itoa(int, char *, int);

inline __attribute__((always_inline)) unsigned int uart0_tx_fifo_free() {
    return TX_FIFO_SIZE - uart0.status.tx_fifo_count;
}

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
    // add '\r' before a newline
    if (c == '\n') {
        // block until we can put a char
        while(uart0_tx_fifo_full());
        uart0.fifo.rw = '\r';
    }
    // block until we can put a char
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

