#include "proc.h"

#define APP_STK 4096
// a 2.3 KB connection struct sits on the frame above the 1.6 KB transmit frame, so 4096 overruns
#define HTTP_STK 8192
#define APP_PRIO 5

void dhcp_proc(void);
void dns_test_proc(void);
void ping_test_proc(void);
void htcpcp_proc(void);
void pulse_proc(void);
void console_test_proc(void);
void gpio_test_proc(void);

void main(void)
{
    spawn(dhcp_proc, APP_STK, APP_PRIO);
    spawn(htcpcp_proc, HTTP_STK, APP_PRIO);
    while (1)
        asm("waiti 0");
}
