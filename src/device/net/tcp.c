#include "timer.h"
#include "def.h"
#include "string.h"
#include "ipv4.h"
#include "tcp.h"
#include "rng.h"
#include "proc.h"

#define LISTEN 0
#define SYN_RCVD 1
#define ESTABLISHED 2
#define CLOSE_WAIT 3
#define FIN_WAIT_1 4
#define FIN_WAIT_2 5
#define LAST_ACK 6

#define FLAG_FIN 0x01
#define FLAG_SYN 0x02
#define FLAG_RST 0x04
#define FLAG_PSH 0x08
#define FLAG_ACK 0x10

#define OPT_MSS 2
#define OPT_MSS_LEN 4

#define TCP_WINDOW (2 * TCP_MSS)

#define RTO_INITIAL 80000000u /* ~1 s */
#define MAX_RETRIES 5

#define SEQ_LT(a, b) ((int)((a) - (b)) < 0)
#define SEQ_LEQ(a, b) ((int)((a) - (b)) <= 0)
#define SEQ_GT(a, b) ((int)((a) - (b)) > 0)
#define SEQ_GEQ(a, b) ((int)((a) - (b)) >= 0)

extern struct ipv4_addr local_ip;

struct tcp_pseudo {
    unsigned int src_addr;
    unsigned int dst_addr;
    unsigned char zero;
    unsigned char protocol;
    unsigned short length;
} __attribute__((packed));

static struct {
    int state;
    unsigned short local_port;
    unsigned short remote_port;
    struct ipv4_addr remote_ip;
    unsigned int snd_una;
    unsigned int snd_nxt;
    unsigned int rcv_nxt;
    u8 tx_flags;
    const u8 *tx_data;
    unsigned int tx_len;
    unsigned int retries;
    unsigned int last_send;
    unsigned int rto_cycles;
    int sender;
    int send_err;
    struct tcp_events ev;
} tcp_ctrl;

static int tcp_ctrl_mutex;

// send_ipv4_raw prepends the ipv4 and snap headers in place
static u8 tx_buf[sizeof(struct tcp_header) + OPT_MSS_LEN + TCP_MSS + 28] __attribute__((aligned(4)));

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

static void send_segment(struct ipv4_addr dst, unsigned short src_port,
                         unsigned short dst_port, unsigned int seq,
                         unsigned int ack, u8 flags, const u8 *data,
                         unsigned int len) {
    struct tcp_header *h = (struct tcp_header *) tx_buf;
    unsigned int hdrlen = sizeof(struct tcp_header);
    h->src_port = htons(src_port);
    h->dst_port = htons(dst_port);
    h->seq = htonl(seq);
    h->ack = htonl(ack);
    h->reserved = 0;
    h->flags = flags;
    h->window = htons(TCP_WINDOW);
    h->checksum = 0;
    h->urgent = 0;
    if (flags & FLAG_SYN) {
        tx_buf[hdrlen] = OPT_MSS;
        tx_buf[hdrlen + 1] = OPT_MSS_LEN;
        tx_buf[hdrlen + 2] = TCP_MSS >> 8;
        tx_buf[hdrlen + 3] = TCP_MSS & 0xff;
        hdrlen += OPT_MSS_LEN;
    }
    h->data_offset = hdrlen / 4;
    memcpy(tx_buf + hdrlen, data, len);

    struct tcp_pseudo p;
    p.src_addr = local_ip.word;
    p.dst_addr = dst.word;
    p.zero = 0;
    p.protocol = IPPROTO_TCP;
    p.length = htons(hdrlen + len);
    unsigned int sum = checksum_partial(0, &p, sizeof(p));
    h->checksum = checksum(tx_buf, hdrlen + len, sum); // tcp has no "no checksum" value, so a zero result goes out as-is

    send_ipv4_raw(local_ip, dst, IPPROTO_TCP, tx_buf, hdrlen + len);
}

static void send_rst(struct ipv4_addr dst, unsigned short src_port,
                     unsigned short dst_port, unsigned int seq) {
    send_segment(dst, src_port, dst_port, seq, 0, FLAG_RST, NULL, 0);
}

static void send(unsigned int seq, u8 flags, const u8 *data, unsigned int len) {
    send_segment(tcp_ctrl.remote_ip, tcp_ctrl.local_port, tcp_ctrl.remote_port,
                 seq, tcp_ctrl.rcv_nxt, flags, data, len);
}

static void send_unacked(void) {
    unsigned int outstanding = tcp_ctrl.snd_nxt - tcp_ctrl.snd_una;
    unsigned int acked = outstanding < tcp_ctrl.tx_len ? tcp_ctrl.tx_len - outstanding : 0;
    send(tcp_ctrl.snd_una, tcp_ctrl.tx_flags, tcp_ctrl.tx_data + acked,
         tcp_ctrl.tx_len - acked);
    tcp_ctrl.last_send = ccount();
}

static void send_new(u8 flags, const u8 *data, unsigned int len) {
    tcp_ctrl.tx_flags = flags;
    tcp_ctrl.tx_data = data;
    tcp_ctrl.tx_len = len;
    tcp_ctrl.retries = 0;
    tcp_ctrl.rto_cycles = RTO_INITIAL;
    send_unacked();
}

static void disconnect(void) {
    unsigned short port = tcp_ctrl.local_port;
    struct tcp_events ev = tcp_ctrl.ev;
    int sender = tcp_ctrl.sender;
    memset(&tcp_ctrl, 0, sizeof(tcp_ctrl));
    tcp_ctrl.local_port = port;
    tcp_ctrl.ev = ev;
    tcp_ctrl.state = LISTEN;
    if (sender) {
        tcp_ctrl.send_err = -1;
        io_signal();
    }
}

static void recv(struct ipv4_addr src, u8 *seg, unsigned int len) {
    struct tcp_header *h = (struct tcp_header *) seg;
    unsigned int dataoff = h->data_offset * 4;
    if (dataoff < sizeof(struct tcp_header) || dataoff > len)
        return;
    unsigned int seq = ntohl(h->seq);
    unsigned int ack = ntohl(h->ack);

    if (tcp_ctrl.state == LISTEN) {
        if (ntohs(h->dst_port) != tcp_ctrl.local_port)
            return;
        if (h->flags & FLAG_RST) // answering a rst with a rst would loop, so drop it
            return;
        if ((h->flags & (FLAG_SYN | FLAG_ACK)) == FLAG_SYN) {
            unsigned int iss = rng_read();
            tcp_ctrl.remote_ip = src;
            tcp_ctrl.remote_port = ntohs(h->src_port);
            tcp_ctrl.rcv_nxt = seq + 1;
            tcp_ctrl.snd_una = iss;
            tcp_ctrl.snd_nxt = iss + 1;
            tcp_ctrl.state = SYN_RCVD;
            send_new(FLAG_SYN | FLAG_ACK, NULL, 0);
        } else if (h->flags & FLAG_ACK) {
            send_rst(src, tcp_ctrl.local_port, ntohs(h->src_port), ack);
        }
        return;
    }

    if (src.word != tcp_ctrl.remote_ip.word
        || ntohs(h->src_port) != tcp_ctrl.remote_port
        || ntohs(h->dst_port) != tcp_ctrl.local_port)
        return;

    if (h->flags & FLAG_RST) {
        if (SEQ_LEQ(tcp_ctrl.rcv_nxt, seq)
            && SEQ_LT(seq, tcp_ctrl.rcv_nxt + TCP_WINDOW)) {
            disconnect();
            tcp_ctrl.ev.closed(-1);
        }
        return;
    }

    if ((h->flags & FLAG_SYN) && tcp_ctrl.state == SYN_RCVD) {
        send_unacked();
        return;
    }

    if ((h->flags & FLAG_ACK) && SEQ_LT(tcp_ctrl.snd_una, ack)
        && SEQ_LEQ(ack, tcp_ctrl.snd_nxt)) {
        tcp_ctrl.snd_una = ack;
        if (tcp_ctrl.snd_una == tcp_ctrl.snd_nxt) {
            if (tcp_ctrl.sender) {
                tcp_ctrl.sender = 0;
                tcp_ctrl.send_err = 0;
                io_signal();
            }
            if (tcp_ctrl.state == SYN_RCVD) {
                tcp_ctrl.state = ESTABLISHED;
                tcp_ctrl.ev.connected();
            } else if (tcp_ctrl.state == FIN_WAIT_1) {
                tcp_ctrl.state = FIN_WAIT_2;
            } else if (tcp_ctrl.state == LAST_ACK) {
                disconnect();
                tcp_ctrl.ev.closed(0);
                return;
            }
        }
    }

    unsigned int plen = len - dataoff;
    if (plen > 0) {
        if (seq != tcp_ctrl.rcv_nxt) {
            send(tcp_ctrl.snd_nxt, FLAG_ACK, NULL, 0);
            return;
        }
        tcp_ctrl.rcv_nxt += plen;
        send(tcp_ctrl.snd_nxt, FLAG_ACK, NULL, 0);
        tcp_ctrl.ev.data(seg + dataoff, plen);
    }

    if (h->flags & FLAG_FIN) {
        if (plen == 0 && seq != tcp_ctrl.rcv_nxt) {
            send(tcp_ctrl.snd_nxt, FLAG_ACK, NULL, 0);
            return;
        }
        if (tcp_ctrl.state == ESTABLISHED) {
            tcp_ctrl.rcv_nxt += 1;
            send(tcp_ctrl.snd_nxt, FLAG_ACK, NULL, 0);
            tcp_ctrl.state = CLOSE_WAIT;
            tcp_ctrl.ev.data(NULL, 0);
        } else if (tcp_ctrl.state == FIN_WAIT_2) {
            tcp_ctrl.rcv_nxt += 1;
            send(tcp_ctrl.snd_nxt, FLAG_ACK, NULL, 0);
            disconnect();
            tcp_ctrl.ev.closed(0);
        }
    }
}

void tcp_recv(struct ipv4_addr src, unsigned char *seg, unsigned int len) {
    mutex_lock(tcp_ctrl_mutex);
    recv(src, seg, len);
    mutex_unlock(tcp_ctrl_mutex);
}

int tcp_send(const unsigned char *buf, unsigned int len) {
    if (len == 0 || len > TCP_MSS)
        return -1;
    mutex_lock(tcp_ctrl_mutex);
    if ((tcp_ctrl.state != ESTABLISHED && tcp_ctrl.state != CLOSE_WAIT)
        || tcp_ctrl.snd_una != tcp_ctrl.snd_nxt) {
        mutex_unlock(tcp_ctrl_mutex);
        return -1;
    }
    tcp_ctrl.sender = 1;
    tcp_ctrl.snd_nxt += len;
    send_new(FLAG_PSH | FLAG_ACK, buf, len);
    while (tcp_ctrl.sender) {
        mutex_unlock(tcp_ctrl_mutex);
        io_wait();
        mutex_lock(tcp_ctrl_mutex);
    }
    int err = tcp_ctrl.send_err;
    mutex_unlock(tcp_ctrl_mutex);
    return err;
}

void tcp_close(void) {
    mutex_lock(tcp_ctrl_mutex);
    if ((tcp_ctrl.state == ESTABLISHED || tcp_ctrl.state == CLOSE_WAIT)
        && tcp_ctrl.snd_una == tcp_ctrl.snd_nxt) {
        tcp_ctrl.state = tcp_ctrl.state == ESTABLISHED ? FIN_WAIT_1 : LAST_ACK;
        tcp_ctrl.snd_nxt += 1;
        send_new(FLAG_FIN | FLAG_ACK, NULL, 0);
    }
    mutex_unlock(tcp_ctrl_mutex);
}

static void tcp_tick(void) {
    mutex_lock(tcp_ctrl_mutex);
    if (tcp_ctrl.snd_una != tcp_ctrl.snd_nxt
        && ccount() - tcp_ctrl.last_send >= tcp_ctrl.rto_cycles) {
        if (tcp_ctrl.retries >= MAX_RETRIES) {
            disconnect();
            tcp_ctrl.ev.closed(-1);
        } else {
            tcp_ctrl.retries += 1;
            tcp_ctrl.rto_cycles <<= 1;
            send_unacked();
        }
    }
    mutex_unlock(tcp_ctrl_mutex);
}

void tcp_timer_proc(void) {
    while (1) {
        io_wait();
        tcp_tick();
    }
}
