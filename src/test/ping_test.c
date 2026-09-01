#include "def.h"
#include "ipv4.h"
#include "icmp.h"
#include "proc.h"

extern struct ipv4_addr local_ip;
extern struct ipv4_addr gw_ip;

#define PERIOD_PING 80000000u /* ~1 s */

static unsigned int ccount(void) {
    unsigned int c;
    asm volatile("rsr.ccount %0" : "=r"(c));
    return c;
}

void ping_test_proc(void) {
    unsigned int seq = 0;
    unsigned int last = ccount() - PERIOD_PING;
    while (1) {
        while (ccount() - last < PERIOD_PING)
            io_wait();
        last = ccount();
        u8 p[sizeof(struct icmp_echo) + 28];
        struct icmp_echo *e = (struct icmp_echo *) p;
        e->type = ICMP_ECHO_REQUEST;
        e->code = 0;
        e->checksum = 0;
        e->ident = htons(1);
        e->seq = htons(seq);
        seq++;
        e->checksum = checksum(e, sizeof(*e), 0);
        send_ipv4_raw(local_ip, gw_ip, IPPROTO_ICMP, p, sizeof(struct icmp_echo));
    }
}
