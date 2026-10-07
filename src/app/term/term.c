#include "def.h"
#include "dev.h"
#include "uart.h"
#include "term.h"

static int uart_echo_enabled(int fd) {
    return 1;
}


void term_proc(void) {
    int fd = open("uart0", 0);
    if (fd < 0) {
        kprintf_uart("term: open failed\n");
        return;
    }
    while (1) {
        term_session(fd, uart_echo_enabled);
    }
}
