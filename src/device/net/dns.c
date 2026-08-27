#include "def.h"
#include "string.h"
#include "ipv4.h"
#include "udp.h"
#include "dns.h"

#define SRC_PORT 4242
#define MAX_NAME 63

extern struct ipv4_addr local_ip;

static struct {
    int busy;
    char name[MAX_NAME + 1];
    struct ipv4_addr server;
    dns_callback cb;
    unsigned short id;
} q;

static unsigned short next_id = 0x1a2b;
static int bound;

/* 12 header + QNAME (name + 2) + 4 question + 36 headroom for send_udp */
static unsigned char pkt[12 + MAX_NAME + 2 + 4 + 36];

static void fail(void) {
    dns_callback cb = q.cb;
    struct ipv4_addr none = {{0, 0, 0, 0}};
    q.busy = 0;
    cb(q.name, none, 0);
}

static int send_query(void) {
    unsigned char *p = pkt;
    memset(pkt, 0, sizeof(pkt));
    p[0] = q.id >> 8;
    p[1] = q.id & 0xff;
    p[2] = 0x01; // recursion desired
    p[5] = 1; // qdcount

    unsigned int n = 12;
    const char *s = q.name;
    while (*s) {
        unsigned int l = 0;
        unsigned char *lenp = &p[n++];
        while (s[l] && s[l] != '.')
            l++;
        memcpy(p + n, s, l);
        *lenp = l;
        n += l;
        s += l;
        if (*s == '.')
            s++;
    }
    p[n++] = 0;
    p[n++] = 0; p[n++] = 1; /* type A */
    p[n++] = 0; p[n++] = 1; /* class IN */

    struct udp_message msg;
    msg.src_ip = local_ip;
    msg.dest_ip = q.server;
    msg.source_port = SRC_PORT;
    msg.dest_port = 53;
    msg.data_length = n;
    msg.data = pkt;
    return send_udp(&msg);
}

static unsigned int skip_name(const unsigned char *m, unsigned int off, unsigned int len) {
    while (off < len) {
        unsigned int l = m[off];
        if (l == 0)
            return off + 1;
        if (l >= 0xc0) // a compression pointer is two bytes and ends the name
            return off + 2;
        off += 1 + l;
    }
    return len;
}

static void dns_rx(struct ipv4_addr src, unsigned int sport,
                   const unsigned char *m, unsigned int len) {
    if (!q.busy || sport != 53 || memcmp(src.bytes, q.server.bytes, 4) != 0)
        return;
    if (len < 12 || ((m[0] << 8) | m[1]) != q.id)
        return;
    if ((m[3] & 0x0f) != 0) {
        fail();
        return;
    }

    unsigned int qd = (m[4] << 8) | m[5];
    unsigned int an = (m[6] << 8) | m[7];

    unsigned int off = 12;
    for (unsigned int i = 0; i < qd && off < len; i++)
        off = skip_name(m, off, len) + 4;

    for (unsigned int i = 0; i < an && off < len; i++) {
        off = skip_name(m, off, len);
        if (off + 10 > len)
            break;
        unsigned int type = (m[off] << 8) | m[off + 1];
        unsigned int rdlen = (m[off + 8] << 8) | m[off + 9];
        off += 10;
        if (type == 1 && rdlen == 4 && off + 4 <= len) {
            dns_callback cb = q.cb;
            struct ipv4_addr a;
            memcpy(a.bytes, m + off, 4);
            q.busy = 0;
            cb(q.name, a, 1);
            return;
        }
        off += rdlen;
    }
    fail();
}

/* Fire-and-forget: the callback runs from the RX path when the reply lands.
   No timeout — a caller that waited long enough re-resolves, which replaces
   the pending query (fresh id, stale replies are ignored). */
int dns_resolve(const char *name, struct ipv4_addr server, dns_callback cb) {
    if (strlen(name) > MAX_NAME)
        return -1;
    if (!bound) {
        if (udp_bind(SRC_PORT, dns_rx) < 0)
            return -1;
        bound = 1;
    }

    q.busy = 1;
    memcpy(q.name, name, strlen(name) + 1);
    q.server = server;
    q.cb = cb;
    q.id = next_id++;
    if (send_query() < 0) {
        fail();
        return -1;
    }
    return 0;
}
