#include "uart.h"
#include "string.h"
#include "proc.h"
#include "tcp.h"

#define ECHO_PORT 80
#define QUIT "quit\n"
#define QUIT_LEN 5
#define LINE_SLOTS 4

struct line {
    unsigned int len;
    unsigned char data[TCP_MSS];
};

static struct line lines[LINE_SLOTS];
static volatile unsigned int line_head;
static volatile unsigned int line_tail;
static int line_sem;
static int line_eof;

static void connected(void) {
    kprintf_uart("tcp: connected\n");
}

static void data(const unsigned char *buf, unsigned int len) {
    if (!buf) {
        line_eof = 1;
        sem_signal(line_sem);
        return;
    }
    if (line_head - line_tail >= LINE_SLOTS)
        return;
    struct line *l = &lines[line_head & (LINE_SLOTS - 1)];
    memcpy(l->data, buf, len);
    l->len = len;
    line_head = line_head + 1;
    sem_signal(line_sem);
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
    line_sem = sem_create(0);
    if (tcp_listen(ECHO_PORT, &ev) < 0)
        kprintf_uart("tcp: listen failed\n");
    while (1) {
        sem_wait(line_sem);
        if (line_head != line_tail) {
            struct line *l = &lines[line_tail & (LINE_SLOTS - 1)];
            if (l->len == QUIT_LEN && memcmp(l->data, QUIT, QUIT_LEN) == 0)
                tcp_close();
            else if (tcp_send(l->data, l->len) < 0)
                kprintf_uart("tcp: send failed\n");
            line_tail = line_tail + 1;
        } else if (line_eof) {
            line_eof = 0;
            tcp_close();
        }
    }
}
