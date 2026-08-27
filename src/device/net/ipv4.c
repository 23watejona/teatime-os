#include "def.h"
#include "string.h"
#include "net.h"
#include "ipv4.h"

#define MAX_PAYLOAD_SIZE (1518 - 20 - 8)

// sums in native order so the result needs no htons; an odd-length region must be the last one summed
unsigned int checksum_partial(unsigned int sum, const void *addr, int count) {
    const unsigned char *p = addr;
    while (count > 1) {
        sum += p[0] | (p[1] << 8);
        p += 2;
        count -= 2;
    }
    if (count > 0)
        sum += p[0];
    return sum;
}

unsigned short checksum(const void *addr, int count, unsigned int start) {
    unsigned int sum = checksum_partial(start, addr, count);
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return (unsigned short)~sum;
}

// payload needs 28 bytes of room past payload_len, since the ipv4 and snap headers are prepended in place
int send_ipv4_raw(struct ipv4_addr src, struct ipv4_addr dst, unsigned char protocol,
                  unsigned char *payload, unsigned int payload_len) {
    if (payload_len > MAX_PAYLOAD_SIZE)
        return -1;

    union ipv4_header h;
    memset(&h, 0, sizeof(h));
    h.fields.version_ihl = (4 << 4) | 5;
    h.fields.total_len = htons(sizeof(h) + payload_len);
    h.fields.flags_frag_off = htons(1 << 14);
    h.fields.ttl = 64;
    h.fields.protocol = protocol;
    h.fields.src_addr = src.word;
    h.fields.dest_addr = dst.word;
    h.fields.checksum = checksum(h.raw, sizeof(h), 0);

    memmove(payload + sizeof(h), payload, payload_len);
    memcpy(payload, h.raw, sizeof(h));
    return net_send_to_gateway(payload, sizeof(h) + payload_len);
}
