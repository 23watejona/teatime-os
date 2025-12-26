#include "uart.h"
#include "reg_util.h"

extern void ctxsw(unsigned int **, unsigned int **);
extern int initmem(void);
extern char *alloc(unsigned int);
extern char *alloc_stack(unsigned int);
extern unsigned int intr_unmask(unsigned int);
extern unsigned int *create(void *, unsigned int);
extern void _set_vec_base(void);

unsigned int *proc1_sp = 0;
unsigned int *proc2_sp = 0;
unsigned int *main_sp = 0;

void null_proc() {
    while (1) {
        BUSY_WAIT();
        kprintf_uart("running null proc\n");
    }
}

void proc1() {
    //int i = 0;
    for (int i = 0; i < 5;) {
        BUSY_WAIT();
        kprintf_uart("proc1 %d\n", i++);
        ctxsw(&proc1_sp, &proc2_sp);
    }
    kprintf_uart("Ending process 1\n");
}

void proc2() {
    int i = 0;
    while (1) {
        BUSY_WAIT();
        kprintf_uart("proc2 %d\n", i++);
        ctxsw(&proc2_sp, &main_sp);
    }
}

void start ( void )
{
    _set_vec_base();
    BUSY_WAIT();
    initmem();
    alloc_stack(512);
    proc1_sp = create(&proc1, 1024);
    proc2_sp = create(proc2, 1024);

    while(1)
    {
        BUSY_WAIT();
        kprintf_uart("Running main\n");
        ctxsw(&main_sp, &proc1_sp);
    }
}
