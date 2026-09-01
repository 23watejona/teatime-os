#include "def.h"
#include "icmp.h"
#include "net.h"

static icmp_echo_reply_handler echo_reply_handler;

int icmp_on_echo_reply(icmp_echo_reply_handler fn) {
    if (echo_reply_handler)
        return -1;
    echo_reply_handler = fn;
    return 0;
}

static void send_echo_reply(u8 *buf, unsigned int ihl, unsigned int total_len,
                            const u8 *sa) {
    struct icmp_echo *e = (struct icmp_echo *) (buf + ihl);
    e->type = ICMP_ECHO_REPLY;
    e->checksum = 0;
    e->checksum = checksum(e, total_len - ihl, 0);

    union ipv4_header *h = (union ipv4_header *) buf;
    unsigned int tmp = h->fields.src_addr;
    h->fields.src_addr = h->fields.dest_addr;
    h->fields.dest_addr = tmp;
    h->fields.ttl = 64;
    h->fields.checksum = 0;
    h->fields.checksum = checksum(h->raw, ihl, 0);
    net_tx(sa, buf, total_len);
}

void icmp_recv(unsigned char *buf, unsigned int ihl, unsigned int total_len,
               const unsigned char *sa) {
    if (total_len - ihl < sizeof(struct icmp_echo))
        return;
    struct icmp_echo *e = (struct icmp_echo *) (buf + ihl);
    if (e->type == ICMP_ECHO_REPLY && echo_reply_handler) {
        struct ipv4_addr src;
        src.word = ((union ipv4_header *) buf)->fields.src_addr;
        echo_reply_handler(src);
    }
    if (e->type == ICMP_ECHO_REQUEST && e->code == 0)
        send_echo_reply(buf, ihl, total_len, sa);
}
