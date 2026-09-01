#include "uart.h"
#include "proc.h"

void dns_test_proc(void);
void ping_test_proc(void);

#define APP_PERIOD_CYCLES 240000000u /* ~3 s at 80 MHz */

static unsigned int ccount(void) {
    unsigned int c;
    asm volatile("rsr.ccount %0" : "=r"(c));
    return c;
}

/* placeholder for the TCP echo app; proves a prio-5 process gets CPU */
static void app_proc(void) {
    unsigned int last = ccount();
    while (1) {
        if (ccount() - last >= APP_PERIOD_CYCLES) {
            kprintf_uart("app: alive\n");
            last = ccount();
        }
    }
}

void main(void)
{
    int pid = create(app_proc, 2048, 5);
    kprintf_uart("main: created app as pid %d\n", pid);
    if (pid >= 0)
        make_avail(pid);

    int dns_test_pid = create(dns_test_proc, 4096, 5);
    if (dns_test_pid >= 0)
        make_avail(dns_test_pid);

    int ping_test_pid = create(ping_test_proc, 4096, 5);
    if (ping_test_pid >= 0)
        make_avail(ping_test_pid);
    while (1)
        asm("waiti 0");
}
