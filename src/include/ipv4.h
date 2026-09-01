#ifndef IPV4_H
#define IPV4_H

#define IPPROTO_ICMP 1
#define IPPROTO_UDP  17

struct ipv4_addr {
    union {
        unsigned char bytes[4];
        unsigned int word;
    };
};

union ipv4_header {
    struct {
        unsigned char version_ihl;
        unsigned char dscp_ecn;
        unsigned short total_len;
        unsigned short ident;
        unsigned short flags_frag_off;
        unsigned char ttl;
        unsigned char protocol;
        unsigned short checksum;
        unsigned int src_addr;
        unsigned int dest_addr;
    } fields;
    unsigned char raw[20];
} __attribute__((packed));

static inline unsigned short htons(unsigned short v) {
    return (unsigned short)((v << 8) | (v >> 8));
}

static inline unsigned int htonl(unsigned int v) {
    return (v << 24) | ((v & 0x0000ff00) << 8) | ((v & 0x00ff0000) >> 8) | (v >> 24);
}

static inline unsigned short ntohs(unsigned short v) {
    return htons(v);
}

static inline unsigned int ntohl(unsigned int v) {
    return htonl(v);
}

unsigned int checksum_partial(unsigned int sum, const void *addr, int count);
unsigned short checksum(const void *addr, int count, unsigned int start);

int send_ipv4_raw(struct ipv4_addr src, struct ipv4_addr dst, unsigned char protocol,
                  unsigned char *payload, unsigned int payload_len);

void ipv4_init(void);

/* Drains the RX ring and runs every L3 receive path in process context, so a
   receive path may block on a send. */
void ipv4_proc(void);

/* Servicer-only producer. Copies the packet (already trimmed to its total
   length) and the sender's link address into the ring; drops when full. */
void ipv4_enqueue(const unsigned char *pkt, unsigned int len,
                  const unsigned char *sa);

#endif
