#ifndef UDP_H
#define UDP_H

#include "ipv4.h"

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

typedef void (*udp_handler)(struct ipv4_addr src, unsigned int sport,
                            const unsigned char *data, unsigned int len);

int send_udp(struct udp_message *msg);

int udp_bind(unsigned short port, udp_handler fn);

void udp_recv(struct ipv4_addr src, struct ipv4_addr dst,
              const unsigned char *dgram, unsigned int len);

#endif
