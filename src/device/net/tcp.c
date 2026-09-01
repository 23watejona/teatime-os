#include "tcp.h"
#include "proc.h"

#define LISTEN 0
#define SYN_RCVD 1
#define ESTABLISHED 2
#define CLOSE_WAIT 3

#define SEQ_LT(a, b) ((int)((a) - (b)) < 0)
#define SEQ_LEQ(a, b) ((int)((a) - (b)) <= 0)
#define SEQ_GT(a, b) ((int)((a) - (b)) > 0)
#define SEQ_GEQ(a, b) ((int)((a) - (b)) >= 0)

static struct {
    int state;
    unsigned short local_port;
    unsigned short remote_port;
    struct ipv4_addr remote_ip;
    unsigned int snd_una;
    unsigned int snd_nxt;
    unsigned int rcv_nxt;
    unsigned int retries;
    unsigned int last_send;
    unsigned int rto_cycles;
    struct tcp_events ev;
} tcp_ctrl;

static int tcp_ctrl_mutex;

void tcp_init(void) {
    tcp_ctrl_mutex = mutex_create();
}

int tcp_listen(unsigned short port, const struct tcp_events *ev) {
    mutex_lock(tcp_ctrl_mutex);
    if (tcp_ctrl.local_port) {
        mutex_unlock(tcp_ctrl_mutex);
        return -1;
    }
    tcp_ctrl.local_port = port;
    tcp_ctrl.ev = *ev;
    tcp_ctrl.state = LISTEN;
    mutex_unlock(tcp_ctrl_mutex);
    return 0;
}

void tcp_recv(struct ipv4_addr src, unsigned char *seg, unsigned int len) {
    struct tcp_header *h = (struct tcp_header *) seg;
    unsigned int dataoff = h->data_offset * 4;
    mutex_lock(tcp_ctrl_mutex);
    if (dataoff < sizeof(struct tcp_header) || dataoff > len) {
        mutex_unlock(tcp_ctrl_mutex);
        return;
    }
    mutex_unlock(tcp_ctrl_mutex);
}

static void tcp_tick(void) {
    mutex_lock(tcp_ctrl_mutex);
    mutex_unlock(tcp_ctrl_mutex);
}

void tcp_timer_proc(void) {
    while (1) {
        io_wait();
        tcp_tick();
    }
}
