#include "timer.h"
#include "def.h"
#include "string.h"
#include "ipv4.h"
#include "tcp.h"
#include "rng.h"
#include "proc.h"
#include "dev.h"

enum tcp_state {
    CLOSED,
    LISTEN,
    SYN_RCVD,
    ESTABLISHED,
    CLOSE_WAIT,
    FIN_WAIT_1,
    FIN_WAIT_2,
    LAST_ACK,
};

#define FLAG_FIN 0x01
#define FLAG_SYN 0x02
#define FLAG_RST 0x04
#define FLAG_PSH 0x08
#define FLAG_ACK 0x10

#define OPT_MSS 2
#define OPT_MSS_LEN 4

#define TCP_WINDOW (2 * TCP_MSS)
#define RX_RING_SIZE 2048

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
    enum tcp_state state;
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
    unsigned int rx_head;
    unsigned int rx_tail;
    int rx_eof;
} tcp_ctrl;

static u8 rx_ring[RX_RING_SIZE];

static int tcp_ctrl_mutex;

// send_ipv4_raw prepends the ipv4 and snap headers in place
static u8 tx_buf[sizeof(struct tcp_header) + OPT_MSS_LEN + TCP_MSS + 28] __attribute__((aligned(4)));

static int connected(void) {
    return tcp_ctrl.state == ESTABLISHED || tcp_ctrl.state == CLOSE_WAIT;
}

static unsigned int rx_room(void) {
    return RX_RING_SIZE - (tcp_ctrl.rx_head - tcp_ctrl.rx_tail);
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
    unsigned int room = rx_room();
    h->window = htons(room < TCP_WINDOW ? room : TCP_WINDOW);
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
    int sender = tcp_ctrl.sender;
    memset(&tcp_ctrl, 0, sizeof(tcp_ctrl));
    tcp_ctrl.local_port = port;
    if (sender)
        tcp_ctrl.send_err = -1;
    io_signal();
}

static void rx_put(const u8 *data, unsigned int len) {
    unsigned int at = tcp_ctrl.rx_head & (RX_RING_SIZE - 1);
    unsigned int first = RX_RING_SIZE - at;
    if (first > len)
        first = len;
    memcpy(rx_ring + at, data, first);
    memcpy(rx_ring, data + first, len - first);
    tcp_ctrl.rx_head += len;
}

static unsigned int rx_get(u8 *out, unsigned int n) {
    unsigned int avail = tcp_ctrl.rx_head - tcp_ctrl.rx_tail;
    if (n > avail)
        n = avail;
    unsigned int at = tcp_ctrl.rx_tail & (RX_RING_SIZE - 1);
    unsigned int first = RX_RING_SIZE - at;
    if (first > n)
        first = n;
    memcpy(out, rx_ring + at, first);
    memcpy(out + first, rx_ring, n - first);
    tcp_ctrl.rx_tail += n;
    return n;
}

static void recv(struct ipv4_addr src, u8 *seg, unsigned int len) {
    struct tcp_header *h = (struct tcp_header *) seg;
    unsigned int dataoff = h->data_offset * 4;
    if (dataoff < sizeof(struct tcp_header) || dataoff > len)
        return;
    unsigned int seq = ntohl(h->seq);
    unsigned int ack = ntohl(h->ack);

    if (tcp_ctrl.state == CLOSED)
        return;

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
            && SEQ_LT(seq, tcp_ctrl.rcv_nxt + TCP_WINDOW))
            disconnect();
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
                io_signal();
            } else if (tcp_ctrl.state == FIN_WAIT_1) {
                tcp_ctrl.state = FIN_WAIT_2;
            } else if (tcp_ctrl.state == LAST_ACK) {
                disconnect();
                return;
            }
        }
    }

    unsigned int plen = len - dataoff;
    if (plen > 0) {
        if (seq != tcp_ctrl.rcv_nxt || plen > rx_room()) {
            send(tcp_ctrl.snd_nxt, FLAG_ACK, NULL, 0);
            return;
        }
        tcp_ctrl.rcv_nxt += plen;
        if (tcp_ctrl.state == ESTABLISHED) // after our fin nobody reads, so the data is acked but not buffered
            rx_put(seg + dataoff, plen);
        send(tcp_ctrl.snd_nxt, FLAG_ACK, NULL, 0);
        io_signal();
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
            tcp_ctrl.rx_eof = 1;
            io_signal();
        } else if (tcp_ctrl.state == FIN_WAIT_2) {
            tcp_ctrl.rcv_nxt += 1;
            send(tcp_ctrl.snd_nxt, FLAG_ACK, NULL, 0);
            disconnect();
        }
    }
}

void tcp_recv(struct ipv4_addr src, unsigned char *seg, unsigned int len) {
    mutex_lock(tcp_ctrl_mutex);
    recv(src, seg, len);
    mutex_unlock(tcp_ctrl_mutex);
}

static int send_data(const u8 *buf, unsigned int len) {
    mutex_lock(tcp_ctrl_mutex);
    if (!connected() || tcp_ctrl.snd_una != tcp_ctrl.snd_nxt) {
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

static int tcp_conn_write(struct dev *d, const void *buf, unsigned int n) {
    const u8 *p = buf;
    unsigned int off = 0;
    while (off < n) {
        unsigned int chunk = n - off;
        if (chunk > TCP_MSS)
            chunk = TCP_MSS;
        if (send_data(p + off, chunk) < 0)
            return -1;
        off += chunk;
    }
    return n;
}

static int tcp_conn_read(struct dev *d, void *buf, unsigned int n) {
    mutex_lock(tcp_ctrl_mutex);
    while (tcp_ctrl.rx_head == tcp_ctrl.rx_tail && !tcp_ctrl.rx_eof
           && tcp_ctrl.state != CLOSED) {
        mutex_unlock(tcp_ctrl_mutex);
        io_wait();
        mutex_lock(tcp_ctrl_mutex);
    }
    int got;
    if (tcp_ctrl.rx_head != tcp_ctrl.rx_tail)
        got = rx_get(buf, n);
    else
        got = tcp_ctrl.rx_eof ? 0 : -1;
    mutex_unlock(tcp_ctrl_mutex);
    return got;
}

static int tcp_conn_close(struct dev *d) {
    mutex_lock(tcp_ctrl_mutex);
    tcp_ctrl.rx_tail = tcp_ctrl.rx_head;
    if (connected() && tcp_ctrl.snd_una == tcp_ctrl.snd_nxt) {
        tcp_ctrl.state = tcp_ctrl.state == ESTABLISHED ? FIN_WAIT_1 : LAST_ACK;
        tcp_ctrl.snd_nxt += 1;
        send_new(FLAG_FIN | FLAG_ACK, NULL, 0);
    }
    while (tcp_ctrl.state != CLOSED) {
        mutex_unlock(tcp_ctrl_mutex);
        io_wait();
        mutex_lock(tcp_ctrl_mutex);
    }
    mutex_unlock(tcp_ctrl_mutex);
    return 0;
}

static int tcp_conn_open(struct dev *d, int arg) {
    return -1;
}

static const struct dev_ops tcp_conn_ops = {
    .open = tcp_conn_open,
    .close = tcp_conn_close,
    .read = tcp_conn_read,
    .write = tcp_conn_write,
};

static int tcp_control(struct dev *d, int op, int arg) {
    int rc = -1;
    mutex_lock(tcp_ctrl_mutex);
    switch (op) {
        case TCP_LISTEN:
            if (!tcp_ctrl.local_port) {
                tcp_ctrl.local_port = arg;
                rc = 0;
            }
            break;
        case TCP_ACCEPT:
            if (!tcp_ctrl.local_port)
                break;
            // the peer may have sent its fin before we run, so close_wait also counts as accepted
            while (!connected()) {
                if (tcp_ctrl.state == CLOSED)
                    tcp_ctrl.state = LISTEN;
                mutex_unlock(tcp_ctrl_mutex);
                io_wait();
                mutex_lock(tcp_ctrl_mutex);
            }
            rc = dev_alloc("tcpconn");
            break;
    }
    mutex_unlock(tcp_ctrl_mutex);
    return rc;
}

static int tcp_close(struct dev *d) {
    int rc = -1;
    mutex_lock(tcp_ctrl_mutex);
    if (tcp_ctrl.state == CLOSED || tcp_ctrl.state == LISTEN) {
        tcp_ctrl.state = CLOSED;
        tcp_ctrl.local_port = 0;
        rc = 0;
    }
    mutex_unlock(tcp_ctrl_mutex);
    return rc;
}

static const struct dev_ops tcp_ops = {
    .close = tcp_close,
    .control = tcp_control,
};

void tcp_init(void) {
    tcp_ctrl_mutex = mutex_create();
    dev_register("tcp", &tcp_ops, NULL);
    dev_register("tcpconn", &tcp_conn_ops, NULL);
}

static void tcp_tick(void) {
    mutex_lock(tcp_ctrl_mutex);
    if (tcp_ctrl.snd_una != tcp_ctrl.snd_nxt
        && ccount() - tcp_ctrl.last_send >= tcp_ctrl.rto_cycles) {
        if (tcp_ctrl.retries >= MAX_RETRIES) {
            disconnect();
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
