#include "def.h"
#include "string.h"
#include "ipv4.h"
#include "udp.h"
#include "dns.h"
#include "dev.h"

#define SRC_PORT 4242
#define DNS_PORT 53
#define MAX_NAME 63
#define DNS_HEADER 12
#define TYPE_A 1
#define CLASS_IN 1

static int fd = -1;
static unsigned short next_id = 0x1a2b;

static u8 query[sizeof(struct udp_datagram) + DNS_HEADER + MAX_NAME + 2 + 4] __attribute__((aligned(4)));
static u8 reply[sizeof(struct udp_datagram) + UDP_DATAGRAM_MAX] __attribute__((aligned(4)));

static unsigned int build_query(u8 *p, const char *name, unsigned short id) {
    memset(p, 0, DNS_HEADER);
    p[0] = id >> 8;
    p[1] = id & 0xff;
    p[2] = 0x01; // recursion desired
    p[5] = 1; // qdcount

    unsigned int n = DNS_HEADER;
    const char *s = name;
    while (*s) {
        unsigned int l = 0;
        u8 *lenp = &p[n++];
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
    p[n++] = 0; p[n++] = TYPE_A;
    p[n++] = 0; p[n++] = CLASS_IN;
    return n;
}

static unsigned int skip_name(const u8 *m, unsigned int off, unsigned int len) {
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

static int parse_reply(const u8 *m, unsigned int len, struct ipv4_addr *addr) {
    if ((m[3] & 0x0f) != 0)
        return -1;
    unsigned int qd = (m[4] << 8) | m[5];
    unsigned int an = (m[6] << 8) | m[7];

    unsigned int off = DNS_HEADER;
    for (unsigned int i = 0; i < qd && off < len; i++)
        off = skip_name(m, off, len) + 4;

    for (unsigned int i = 0; i < an && off < len; i++) {
        off = skip_name(m, off, len);
        if (off + 10 > len)
            break;
        unsigned int type = (m[off] << 8) | m[off + 1];
        unsigned int rdlen = (m[off + 8] << 8) | m[off + 9];
        off += 10;
        if (type == TYPE_A && rdlen == 4 && off + 4 <= len) {
            memcpy(addr->bytes, m + off, 4);
            return 0;
        }
        off += rdlen;
    }
    return -1;
}

int dns_resolve(const char *name, struct ipv4_addr server, struct ipv4_addr *addr) {
    if (strlen(name) > MAX_NAME)
        return -1;
    if (fd < 0) {
        fd = open("udp", 0);
        if (fd < 0)
            return -1;
        if (control(fd, UDP_BIND, SRC_PORT) < 0) {
            close(fd);
            fd = -1;
            return -1;
        }
    }

    unsigned short id = next_id++;
    struct udp_datagram *q = (struct udp_datagram *) query;
    q->addr = server;
    q->port = DNS_PORT;
    q->len = build_query(q->data, name, id);
    if (write(fd, query, sizeof(*q) + q->len) < 0)
        return -1;

    while (1) {
        if (read(fd, reply, sizeof(reply)) < 0)
            return -1;
        struct udp_datagram *r = (struct udp_datagram *) reply;
        if (r->addr.word != server.word || r->port != DNS_PORT)
            continue;
        if (r->len < DNS_HEADER || ((r->data[0] << 8) | r->data[1]) != id)
            continue;
        return parse_reply(r->data, r->len, addr);
    }
}
