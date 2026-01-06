#include "uart.h"
#include "proc.h"

extern void yield(void);


void start_proc(void (*func)(void), int pid) {
    func();
    proctab[pid].status = PROC_UNUSED;
    yield();
}
