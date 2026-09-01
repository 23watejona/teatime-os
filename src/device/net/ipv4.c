#include "def.h"
#include "string.h"
#include "net.h"
#include "ipv4.h"
#include "udp.h"
#include "tcp.h"
#include "icmp.h"
#include "proc.h"

#define MAX_PAYLOAD_SIZE (1518 - 20 - 8)
#define IPV4_RING_SLOTS 4
#define IPV4_MTU 1500

extern struct ipv4_addr local_ip;


// the servicer copies each packet in behind 8 bytes of headroom, so a reply can be built in place without a second copy
struct ipv4_slot {
    u8 sa[6];
    unsigned int len;
    u8 data[8 + IPV4_MTU] __attribute__((aligned(4)));
};

static struct ipv4_slot ring[IPV4_RING_SLOTS];
static volatile unsigned int ring_head;
static volatile unsigned int ring_tail;
static int ring_sem;

void ipv4_init(void) {
    ring_sem = sem_create(0);
}

// sums in native order so the result needs no htons; an odd-length region must be the last one summed
unsigned int checksum_partial(unsigned int sum, const void *addr, int count) {
    const unsigned char *p = addr;
    while (count > 1) {
        sum += p[0] | (p[1] << 8);
        p += 2;
        count -= 2;
    }
    if (count > 0)
        sum += p[0];
    return sum;
}

unsigned short checksum(const void *addr, int count, unsigned int start) {
    unsigned int sum = checksum_partial(start, addr, count);
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return (unsigned short)~sum;
}

static void ipv4_recv(u8 *buf, unsigned int len, const u8 *sa) {
    union ipv4_header *h = (union ipv4_header *) buf;
    unsigned int ihl = (h->fields.version_ihl & 0x0f) * 4;
    if (ihl < sizeof(union ipv4_header) || len < ihl)
        return;
    unsigned int payload_len = len - ihl;
    u8 *payload = buf + ihl;
    struct ipv4_addr src;
    struct ipv4_addr dst;
    src.word = h->fields.src_addr;
    dst.word = h->fields.dest_addr;
    int to_us = dst.word == local_ip.word;
    switch (h->fields.protocol) {
        case IPPROTO_ICMP:
            if (to_us)
                icmp_recv(buf, ihl, len, sa);
            break;
        case IPPROTO_TCP:
            if (to_us && payload_len >= sizeof(struct tcp_header))
                tcp_recv(src, payload, payload_len);
            break;
        case IPPROTO_UDP:
            if (to_us && payload_len >= sizeof(union udp_header))
                udp_recv(src, dst, payload, payload_len);
            break;
    }
}

void ipv4_enqueue(const unsigned char *pkt, unsigned int len,
                  const unsigned char *sa) {
    if (len > IPV4_MTU)
        return;
    if (ring_head - ring_tail >= IPV4_RING_SLOTS)
        return;
    struct ipv4_slot *s = &ring[ring_head & (IPV4_RING_SLOTS - 1)];
    memcpy(s->sa, sa, 6);
    memcpy(s->data + 8, pkt, len);
    s->len = len;
    ring_head = ring_head + 1;
    sem_signal(ring_sem);
}

void ipv4_proc(void) {
    while (1) {
        sem_wait(ring_sem);
        struct ipv4_slot *s = &ring[ring_tail & (IPV4_RING_SLOTS - 1)];
        ipv4_recv(s->data + 8, s->len, s->sa);
        ring_tail = ring_tail + 1;
    }
}

// payload needs 28 bytes of room past payload_len, since the ipv4 and snap headers are prepended in place
int send_ipv4_raw(struct ipv4_addr src, struct ipv4_addr dst, unsigned char protocol,
                  unsigned char *payload, unsigned int payload_len) {
    if (payload_len > MAX_PAYLOAD_SIZE)
        return -1;

    union ipv4_header h;
    memset(&h, 0, sizeof(h));
    h.fields.version_ihl = (4 << 4) | 5;
    h.fields.total_len = htons(sizeof(h) + payload_len);
    h.fields.flags_frag_off = htons(1 << 14);
    h.fields.ttl = 64;
    h.fields.protocol = protocol;
    h.fields.src_addr = src.word;
    h.fields.dest_addr = dst.word;
    h.fields.checksum = checksum(h.raw, sizeof(h), 0);

    memmove(payload + sizeof(h), payload, payload_len);
    memcpy(payload, h.raw, sizeof(h));
    return net_send(dst, payload, sizeof(h) + payload_len);
}
