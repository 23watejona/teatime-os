#include "def.h"
#include "string.h"
#include "ipv4.h"
#include "udp.h"

#define IP_PROTO_UDP 17

#define MAX_BINDS 4
static struct {
    unsigned short port;
    udp_handler fn;
} binds[MAX_BINDS];

int udp_bind(unsigned short port, udp_handler fn) {
    for (int i = 0; i < MAX_BINDS; i++) {
        if (!binds[i].fn) {
            binds[i].port = port;
            binds[i].fn = fn;
            return 0;
        }
    }
    return -1;
}

// msg->data needs 36 bytes of room past data_length, since the udp, ipv4 and snap headers are prepended in place
int send_udp(struct udp_message *msg) {
    unsigned int len = sizeof(union udp_header) + msg->data_length;

    union udp_header h;
    h.fields.source_port = htons(msg->source_port);
    h.fields.dest_port = htons(msg->dest_port);
    h.fields.length = htons(len);
    h.fields.checksum = 0;

    unsigned char pseudo[12];
    memcpy(pseudo, msg->src_ip.bytes, 4);
    memcpy(pseudo + 4, msg->dest_ip.bytes, 4);
    pseudo[8] = 0;
    pseudo[9] = IP_PROTO_UDP;
    pseudo[10] = len >> 8;
    pseudo[11] = len & 0xff;

    unsigned int sum = checksum_partial(0, pseudo, sizeof(pseudo));
    sum = checksum_partial(sum, h.raw, sizeof(h));
    unsigned short c = checksum(msg->data, msg->data_length, sum);
    h.fields.checksum = c ? c : 0xffff; // a zero checksum means none on the wire, so send its other representation

    memmove(msg->data + sizeof(h), msg->data, msg->data_length);
    memcpy(msg->data, h.raw, sizeof(h));

    return send_ipv4_raw(msg->src_ip, msg->dest_ip, IP_PROTO_UDP, msg->data, len);
}

void udp_recv(struct ipv4_addr src, struct ipv4_addr dst,
              const unsigned char *dgram, unsigned int len) {
    (void)dst;
    if (len < 8)
        return;
    // the frame may carry padding, so the udp length field bounds the payload, not len
    unsigned int dlen = (dgram[4] << 8) | dgram[5];
    if (dlen < 8 || dlen > len)
        return;

    unsigned int sport = (dgram[0] << 8) | dgram[1];
    unsigned int dport = (dgram[2] << 8) | dgram[3];
    for (int i = 0; i < MAX_BINDS; i++) {
        if (binds[i].fn && binds[i].port == dport) {
            binds[i].fn(src, sport, dgram + 8, dlen - 8);
            return;
        }
    }
}
