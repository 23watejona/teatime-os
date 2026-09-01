#ifndef TCP_H
#define TCP_H

#include "ipv4.h"

#define TCP_MSS 536

/* control ops on the "tcp" listener. The accepted "tcpconn": read returns 0
   at the peer's FIN and -1 once the connection is gone; write blocks until
   acked, -1 if the connection dies; close sends our FIN and blocks until the
   connection is fully down. */
#define TCP_LISTEN 1 /* arg = local port */
#define TCP_ACCEPT 2 /* blocks until a connection is established; returns its descriptor */

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

void tcp_recv(struct ipv4_addr src, unsigned char *seg, unsigned int len);

void tcp_init(void);

void tcp_timer_proc(void);

#endif
