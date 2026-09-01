#include "def.h"
#include "icmp.h"

static icmp_echo_reply_handler echo_reply_handler;

int icmp_on_echo_reply(icmp_echo_reply_handler fn) {
    if (echo_reply_handler)
        return -1;
    echo_reply_handler = fn;
    return 0;
}

void icmp_recv(struct ipv4_addr src, unsigned char *payload, unsigned int len) {
    if (len < sizeof(struct icmp_echo))
        return;
    struct icmp_echo *e = (struct icmp_echo *) payload;
    if (e->type == ICMP_ECHO_REPLY && echo_reply_handler)
        echo_reply_handler(src);
}
