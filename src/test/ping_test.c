#include "def.h"
#include "ipv4.h"
#include "icmp.h"
#include "proc.h"
#include "timer.h"

extern struct ipv4_addr local_ip;
extern struct ipv4_addr gw_ip;

void ping_test_proc(void) {
    unsigned int seq = 0;
    while (1) {
        sleep(TICKS_PER_SEC);
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
