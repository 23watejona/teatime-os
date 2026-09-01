#include "proc.h"
#include "mem.h"

void start_proc(void (*func)(void), int pid) {
    func();
    disable();
    // free() overwrites the canary word, so the status must be UNUSED before sched sweeps
    free(proctab[pid].stk_base);
    proctab[pid].status = PROC_UNUSED;
    sched();
}
