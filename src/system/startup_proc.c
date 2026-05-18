#include "proc.h"
#include "uart.h"
#include "reg_util.h"

#define NULL_STK 512
#define INIT_STK 1024

void make_avail(int);
extern void main(void);
void init_cpu_timer(void);
void sched(void);
void intr_unmask(int);

/*void startup_proc () {
    int pid = create(main, INIT_STK, 5);
    init_cpu_timer();
    make_avail(pid);
    sched();
    kprintf_uart("running null proc\n");
    
    intr_unmask(1 << 6);
    while (1) {
        asm("waiti 0");
    }
}*/
