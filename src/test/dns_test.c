#include "uart.h"
#include "ipv4.h"
#include "icmp.h"
#include "dns.h"
#include "proc.h"

static void dns_test_send(void) {
    struct ipv4_addr resolver = {{1, 1, 1, 1}};
    struct ipv4_addr a;
    kprintf_uart("dns: A? google.com -> 1.1.1.1\n");
    if (dns_resolve("google.com", resolver, &a) == 0)
        kprintf_uart("dns: google.com = %u.%u.%u.%u\n",
                     a.bytes[0], a.bytes[1], a.bytes[2], a.bytes[3]);
    else
        kprintf_uart("dns: google.com lookup FAILED\n");
}

static volatile unsigned int gateway_answered;

static void echo_reply_recv(struct ipv4_addr src) {
    gateway_answered = 1;
}

void dns_test_proc(void) {
    icmp_on_echo_reply(echo_reply_recv);
    while (!gateway_answered)
        io_wait();
    dns_test_send();
    while (1)
        io_wait();
}
