#include "uart.h"
#include "proc.h"

void dns_test_proc(void);
void ping_test_proc(void);
void htcpcp_proc(void);
void console_test_proc(void);

void main(void)
{
    int dns_test_pid = create(dns_test_proc, 4096, 5);
    if (dns_test_pid >= 0)
        make_avail(dns_test_pid);

    int ping_test_pid = create(ping_test_proc, 4096, 5);
    if (ping_test_pid >= 0)
        make_avail(ping_test_pid);

    int htcpcp_pid = create(htcpcp_proc, 4096, 5);
    if (htcpcp_pid >= 0)
        make_avail(htcpcp_pid);

    int console_test_pid = create(console_test_proc, 4096, 5);
    if (console_test_pid >= 0)
        make_avail(console_test_pid);
    while (1)
        asm("waiti 0");
}
