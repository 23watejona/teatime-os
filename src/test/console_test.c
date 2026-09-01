#include "dev.h"
#include "uart.h"

#define ECHO_BUF 32

void console_test_proc(void) {
    int fd = open("uart0", 0);
    if (fd < 0) {
        kprintf_uart("console: open failed\n");
        return;
    }
    unsigned char buf[ECHO_BUF];
    while (1) {
        int n = read(fd, buf, sizeof(buf));
        if (n > 0)
            write(fd, buf, n);
    }
}
