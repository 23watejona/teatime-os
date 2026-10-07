#include "ipv4.h"
#include "term.h"
#include "term_command.h"

static void put_ip(struct term *t, const char *label, struct ipv4_addr a) {
    term_put(t, label);
    for (unsigned int i = 0; i < sizeof(a.bytes); i++) {
        if (i) {
            term_put(t, ".");
        }
        term_put_number(t, a.bytes[i]);
    }
    term_put(t, TERM_NEWLINE);
}

static void run(struct term *t, int argc, char **argv) {
    put_ip(t, "ip ", local_ip);
    put_ip(t, "gw ", gw_ip);
    put_ip(t, "mask ", net_mask);
}

const struct term_command ip_command = { "ip", "local address, gateway and mask", run };
