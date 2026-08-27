#include "uart.h"
#include "ipv4.h"
#include "dns.h"

static void dns_result(const char *name, struct ipv4_addr a, int ok) {
    if (ok)
        kprintf_uart("dns: %s = %u.%u.%u.%u\n", name,
                     a.bytes[0], a.bytes[1], a.bytes[2], a.bytes[3]);
    else
        kprintf_uart("dns: %s lookup FAILED\n", name);
}

void dns_test_send(void) {
    struct ipv4_addr resolver = {{1, 1, 1, 1}};
    kprintf_uart("dns: A? google.com -> 1.1.1.1\n");
    if (dns_resolve("google.com", resolver, dns_result) < 0)
        kprintf_uart("dns: resolver busy\n");
}
