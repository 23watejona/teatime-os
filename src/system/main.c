#include "uart.h"
#include "reg_util.h"
#include "proc.h"

extern void yield(void);
extern void make_avail(unsigned int);

void proc1() {
    for (int i = 0; i < 2;) {
        BUSY_WAIT();
        kprintf_uart("proc1 %d\n", i++);
        yield();
    }
    kprintf_uart("proc1: last call\n");
}

void proc2() {
    for (int i = 0; i < 3; ++i) {
        BUSY_WAIT();
        kprintf_uart("proc2 %d\n", i);
        yield();
    }
    kprintf_uart("proc2: last call\n");
}

void main ( void )
{
    make_avail(create(proc1, 1024, 5));
    make_avail(create(proc2, 1024, 5));

    for(int j = 0; j < 5; ++j)
    {
        BUSY_WAIT();
        kprintf_uart("main\n");
        yield();
    }
    kprintf_uart("main: last call\n");
}
