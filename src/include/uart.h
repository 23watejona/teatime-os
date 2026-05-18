#ifndef UART_H
#define UART_H

#define UART_DEFAULT_BAUD (76800u)

void kprintf_uart(char *f, ...);
void kputc_uart(int c);

struct uart_ctrl {
    struct {
        char rw;
        char reserved[3]; // pad to 4 bytes
    } fifo;
    struct {
        char rxfifo_full: 1;
        char txfifo_empty: 1;
        char parity_err: 1;
        char frm_err: 1;
        char rxfifo_overflow: 1;
        char dsr_change: 1;
        char cts_change: 1;
        char brk_detected: 1;
        char rxfifo_timeout: 1;
        unsigned int reserved: 23;
    } int_raw;
    struct {
        char rxfifo_full: 1;
        char txfifo_empty: 1;
        char parity_err: 1;
        char frm_err: 1;
        char rxfifo_overflow: 1;
        char dsr_change: 1;
        char cts_change: 1;
        char brk_detected: 1;
        char rxfifo_timeout: 1;
        unsigned int reserved: 23;
    } int_status;

    struct {
        char rxfifo_full: 1;
        char txfifo_empty: 1;
        char parity_err: 1;
        char frm_err: 1;
        char rxfifo_overflow: 1;
        char dsr_change: 1;
        char cts_change: 1;
        char brk_detected: 1;
        char rxfifo_timeout: 1;
        unsigned int reserved: 23;
    } int_enable; // enable/disable interrupts
    
    struct {
        char rxfifo_full: 1;
        char txfifo_empty: 1;
        char parity_err: 1;
        char frm_err: 1;
        char rxfifo_overflow: 1;
        char dsr_change: 1;
        char cts_change: 1;
        char brk_detected: 1;
        char rxfifo_timeout: 1;
        unsigned int reserved: 23;
    } int_clear; // clear interrupt status

    struct {
        unsigned int div_int: 20;
        unsigned int reserved: 12;
    } clk_div; // set clock div

    struct {
        unsigned int enable: 1;
        unsigned int reserved1: 7;
        unsigned int glitch_filter: 8;
        unsigned int reserved2: 16;
    } auto_baud;

    struct {
        unsigned int rx_fifo_count: 8;
        unsigned int reserved1: 5;
        unsigned int dsrn: 1;
        unsigned int ctsn: 1;
        unsigned int rxd: 1;
        unsigned int tx_fifo_count: 8;
        unsigned int reserved2: 5;
        unsigned int dtrn: 1;
        unsigned int rtsn: 1;
        unsigned int txd: 1;
    } status;

    struct {
        unsigned int x;

    } conf0;

    struct {
        unsigned int x;

    } conf1;

    struct {
        unsigned int min_duration: 20;
        unsigned int reserved: 12;

    } low_pulse;

    struct {
        unsigned int min_duration: 20;
        unsigned int reserved: 12;
    } high_pulse;

    struct {
        unsigned int count: 10;
        unsigned int reserved: 22;

    } rxd_count;

    unsigned int reserved[17];
    unsigned int date;
    unsigned int id;
};

extern struct uart_ctrl uart0;

#define TX_FIFO_SIZE (127) // it's actually 128 but we give it a byte of wiggle
#define RX_FIFO_SIZE (127) // it's actually 128 but we give it a byte of wiggle

#endif
