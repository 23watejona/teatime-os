#include "uart.h"
#include "reg_util.h"
#include "proc.h"


void init_cpu_timer(void);
void make_avail(int pid);

void proc2 (void) {
    for (int i = 0; i < 10; ++i) {
        kprintf_uart("running proc2\n");
        BUSY_WAIT();
    }
}

void main ( void )
{
    make_avail(create(proc2, 512, 5));
    for (int i = 0; i < 20; ++i) {
        kprintf_uart("running main\n");
        BUSY_WAIT();
    }
}
