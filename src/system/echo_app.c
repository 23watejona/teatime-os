#include "uart.h"
#include "proc.h"
#include "tcp.h"

#define ECHO_PORT 80

static void connected(void) {
    kprintf_uart("tcp: connected\n");
}

static void data(const unsigned char *buf, unsigned int len) {
    if (buf)
        kprintf_uart("tcp: data %u\n", len);
    else
        kprintf_uart("tcp: eof\n");
}

static void closed(int err) {
    kprintf_uart("tcp: closed %d\n", err);
}

static const struct tcp_events ev = {
    .connected = connected,
    .data = data,
    .closed = closed,
};

void echo_app_proc(void) {
    if (tcp_listen(ECHO_PORT, &ev) < 0)
        kprintf_uart("tcp: listen failed\n");
    while (1)
        io_wait();
}
