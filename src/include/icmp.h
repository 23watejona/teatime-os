#ifndef ICMP_H
#define ICMP_H

#include "ipv4.h"

#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY   0

struct icmp_echo {
    unsigned char type;
    unsigned char code;
    unsigned short checksum;
    unsigned short ident;
    unsigned short seq;
} __attribute__((packed));

typedef void (*icmp_echo_reply_handler)(struct ipv4_addr src);

int icmp_on_echo_reply(icmp_echo_reply_handler fn);

void icmp_recv(struct ipv4_addr src, unsigned char *payload, unsigned int len);

#endif
