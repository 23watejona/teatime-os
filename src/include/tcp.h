#ifndef TCP_H
#define TCP_H

#include "ipv4.h"

struct tcp_header {
    unsigned short src_port;
    unsigned short dst_port;
    unsigned int seq;
    unsigned int ack;
    unsigned char reserved:4;
    unsigned char data_offset:4;
    unsigned char flags;
    unsigned short window;
    unsigned short checksum;
    unsigned short urgent;
} __attribute__((packed));

/* Callbacks run in the IPv4 handler process under the TCP lock and must not
   block. data(NULL, 0) = peer EOF; closed err < 0 = RST or retransmit give-up. */
struct tcp_events {
    void (*connected)(void);
    void (*data)(const unsigned char *buf, unsigned int len);
    void (*closed)(int err);
};

int tcp_listen(unsigned short port, const struct tcp_events *ev);

void tcp_recv(struct ipv4_addr src, unsigned char *seg, unsigned int len);

void tcp_init(void);

void tcp_timer_proc(void);

#endif
