#ifndef UDP_H
#define UDP_H

#include "ipv4.h"

/* control ops on a "udp" descriptor: UDP_BIND arg = local port;
   UDP_TIMEOUT arg = ticks a read waits before returning -1, 0 waits forever */
#define UDP_BIND 1
#define UDP_TIMEOUT 2
#define UDP_DATAGRAM_MAX 512

/* read and write both move one of these. On read addr/port are the sender
   and len the payload copied; on write they are the destination and the
   payload length, and n must cover them. */
struct udp_datagram {
    struct ipv4_addr addr;
    unsigned short port;
    unsigned short len;
    unsigned char data[];
};

struct udp_message {
    struct ipv4_addr src_ip;
    struct ipv4_addr dest_ip;
    unsigned short source_port;
    unsigned short dest_port;
    unsigned short data_length;
    unsigned char *data;
};

union udp_header {
    struct {
        unsigned short source_port;
        unsigned short dest_port;
        unsigned short length;
        unsigned short checksum;
    } fields;
    unsigned char raw[8];
} __attribute__((packed));

void udp_init(void);

int send_udp(struct udp_message *msg);

void udp_recv(struct ipv4_addr src, struct ipv4_addr dst,
              const unsigned char *dgram, unsigned int len);

#endif
