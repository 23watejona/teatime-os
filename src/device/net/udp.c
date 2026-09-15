#include "def.h"
#include "string.h"
#include "ipv4.h"
#include "udp.h"
#include "dev.h"
#include "proc.h"

#define IP_PROTO_UDP 17

#define MAX_BINDS 4
#define UDP_RING_SLOTS 2

extern struct ipv4_addr local_ip;

struct udp_slot {
    struct ipv4_addr addr;
    unsigned short port;
    unsigned short len;
    u8 data[UDP_DATAGRAM_MAX];
};

struct udp_bind {
    int used;
    unsigned short port;
    unsigned int timeout;
    struct dev *dev;
    struct udp_slot ring[UDP_RING_SLOTS];
    unsigned int head;
    unsigned int tail;
    u8 tx[sizeof(union udp_header) + UDP_DATAGRAM_MAX + 28] __attribute__((aligned(4)));
};

static struct udp_bind binds[MAX_BINDS];
static int udp_mutex;

static int udp_open(struct dev *d, int arg) {
    struct udp_bind *b = d->state;
    b->port = 0;
    b->timeout = 0;
    b->head = 0;
    b->tail = 0;
    b->used = 1;
    return 0;
}

static int udp_close(struct dev *d) {
    struct udp_bind *b = d->state;
    b->used = 0;
    return 0;
}

static int udp_control(struct dev *d, int op, int arg) {
    struct udp_bind *b = d->state;
    if (op == UDP_TIMEOUT) {
        b->timeout = arg;
        return 0;
    }
    if (op != UDP_BIND)
        return -1;
    for (int i = 0; i < MAX_BINDS; i++)
        if (binds[i].used && binds[i].port == arg)
            return -1;
    b->port = arg;
    return 0;
}

static int udp_read(struct dev *d, void *buf, unsigned int n) {
    struct udp_bind *b = d->state;
    struct udp_datagram *dg = buf;
    if (n < sizeof(*dg))
        return -1;
    mutex_lock(udp_mutex);
    while (b->head == b->tail) {
        if (!b->timeout) {
            cond_wait(b->dev->cond, udp_mutex);
        } else if (cond_timedwait(b->dev->cond, udp_mutex, b->timeout) < 0) {
            mutex_unlock(udp_mutex);
            return -1;
        }
    }
    struct udp_slot *slot = &b->ring[b->tail & (UDP_RING_SLOTS - 1)];
    unsigned int len = slot->len;
    if (len > n - sizeof(*dg))
        len = n - sizeof(*dg);
    dg->addr = slot->addr;
    dg->port = slot->port;
    dg->len = len;
    memcpy(dg->data, slot->data, len);
    b->tail = b->tail + 1;
    mutex_unlock(udp_mutex);
    return sizeof(*dg) + len;
}

// msg->data needs 36 bytes of room past data_length, since the udp, ipv4 and snap headers are prepended in place
int send_udp(struct udp_message *msg) {
    unsigned int len = sizeof(union udp_header) + msg->data_length;

    union udp_header h;
    h.fields.source_port = htons(msg->source_port);
    h.fields.dest_port = htons(msg->dest_port);
    h.fields.length = htons(len);
    h.fields.checksum = 0;

    unsigned char pseudo[12];
    memcpy(pseudo, msg->src_ip.bytes, 4);
    memcpy(pseudo + 4, msg->dest_ip.bytes, 4);
    pseudo[8] = 0;
    pseudo[9] = IP_PROTO_UDP;
    pseudo[10] = len >> 8;
    pseudo[11] = len & 0xff;

    unsigned int sum = checksum_partial(0, pseudo, sizeof(pseudo));
    sum = checksum_partial(sum, h.raw, sizeof(h));
    unsigned short c = checksum(msg->data, msg->data_length, sum);
    h.fields.checksum = c ? c : 0xffff; // a zero checksum means none on the wire, so send its other representation

    memmove(msg->data + sizeof(h), msg->data, msg->data_length);
    memcpy(msg->data, h.raw, sizeof(h));

    return send_ipv4_raw(msg->src_ip, msg->dest_ip, IP_PROTO_UDP, msg->data, len);
}

static int udp_write(struct dev *d, const void *buf, unsigned int n) {
    struct udp_bind *b = d->state;
    const struct udp_datagram *dg = buf;
    if (!b->port || n < sizeof(*dg) || dg->len > UDP_DATAGRAM_MAX
        || n < sizeof(*dg) + dg->len)
        return -1;
    memcpy(b->tx, dg->data, dg->len);
    struct udp_message msg;
    msg.src_ip = local_ip;
    msg.dest_ip = dg->addr;
    msg.source_port = b->port;
    msg.dest_port = dg->port;
    msg.data_length = dg->len;
    msg.data = b->tx;
    if (send_udp(&msg) < 0)
        return -1;
    return sizeof(*dg) + dg->len;
}

static const struct dev_ops udp_ops = {
    .open = udp_open,
    .close = udp_close,
    .read = udp_read,
    .write = udp_write,
    .control = udp_control,
};

void udp_init(void) {
    udp_mutex = mutex_create();
    for (int i = 0; i < MAX_BINDS; i++) {
        binds[i].dev = dev_register("udp", &udp_ops, &binds[i]);
    }
}

void udp_recv(struct ipv4_addr src, struct ipv4_addr dst,
              const unsigned char *dgram, unsigned int len) {
    (void)dst;
    if (len < 8)
        return;
    // the frame may carry padding, so the udp length field bounds the payload, not len
    unsigned int dlen = (dgram[4] << 8) | dgram[5];
    if (dlen < 8 || dlen > len)
        return;

    unsigned int sport = (dgram[0] << 8) | dgram[1];
    unsigned int dport = (dgram[2] << 8) | dgram[3];
    unsigned int plen = dlen - 8;
    mutex_lock(udp_mutex);
    for (int i = 0; i < MAX_BINDS; i++) {
        struct udp_bind *b = &binds[i];
        if (!b->used || b->port != dport)
            continue;
        if (b->head - b->tail >= UDP_RING_SLOTS || plen > UDP_DATAGRAM_MAX)
            break;
        struct udp_slot *slot = &b->ring[b->head & (UDP_RING_SLOTS - 1)];
        slot->addr = src;
        slot->port = sport;
        slot->len = plen;
        memcpy(slot->data, dgram + 8, plen);
        b->head = b->head + 1;
        cond_broadcast(b->dev->cond);
        break;
    }
    mutex_unlock(udp_mutex);
}
