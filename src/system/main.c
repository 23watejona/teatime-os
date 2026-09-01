#include "proc.h"

#define APP_STK 4096
#define APP_PRIO 5

void dns_test_proc(void);
void ping_test_proc(void);
void htcpcp_proc(void);
void console_test_proc(void);
void gpio_test_proc(void);

void main(void)
{
    spawn(htcpcp_proc, APP_STK, APP_PRIO);
    spawn(dns_test_proc, APP_STK, APP_PRIO);
    spawn(ping_test_proc, APP_STK, APP_PRIO);
    spawn(console_test_proc, APP_STK, APP_PRIO);
    spawn(gpio_test_proc, APP_STK, APP_PRIO);
    while (1)
        asm("waiti 0");
}
