#include "dev.h"
#include "tcp.h"
#include "telnet.h"
#include "term.h"
#include "uart.h"

#define TELNET_PORT 23

static int telnet_echo_enabled(int fd) {
    return control(fd, TELNET_LOCAL_ENABLED, TELNET_OPTION_ECHO) == 1;
}

void telnet_proc(void) {
    int listener = open("tcp", 0);
    if (listener < 0 || control(listener, TCP_LISTEN, TELNET_PORT) < 0) {
        kprintf_uart("telnet: listen failed\n");
        return;
    }
    while (1) {
        int conn = control(listener, TCP_ACCEPT, 0);
        if (conn < 0) {
            continue;
        }
        int fd = open("telnet", conn);
        if (fd < 0) {
            close(conn);
            continue;
        }
        term_session(fd, telnet_echo_enabled);
        close(fd);
    }
}
