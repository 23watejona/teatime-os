#include "def.h"
#include "proc.h"
#include "uart.h"
#include "proc_queue.h"
#include "wdt.h"

extern unsigned int _nmi_stack_bottom;
extern void ctxsw(unsigned int **, unsigned int **);
extern queue_entry *avail_list;

// not iram: excm is clear in handler context, so a flash call from sched is fine
static void stk_smash(int pid, unsigned int val) {
    kprintf_uart("\nstack canary smashed: pid %d val=%x -- rebooting\n", pid, val);
    system_reboot();
}

IRAM_ATTR void sched() {
    int m = disable();

    for (int i = 0; i < NUM_PROC; ++i) {
        if (proctab[i].status != PROC_UNUSED && proctab[i].stk_base &&
            *proctab[i].stk_base != STK_CANARY(i))
            stk_smash(i, *proctab[i].stk_base);
    }
    if (_nmi_stack_bottom != STK_CANARY(NUM_PROC)) // the nmi stack has no proctab row, so its canary takes the pid after the last
        stk_smash(NUM_PROC, _nmi_stack_bottom);

    proctab_entry *old_proc = &proctab[curr_pid];
    proctab_entry *new_proc = NULL;

    if (proctab[curr_pid].status == PROC_CURR) {
        proctab[curr_pid].status = PROC_AVAIL;
        proc_enqueue(avail_list, curr_pid, proctab[curr_pid].priority);
    }
    curr_pid = proc_dequeue(avail_list);
    new_proc = &proctab[curr_pid];
    proctab[curr_pid].status = PROC_CURR;
    
    // no need to context switch if it's the same process
    if (old_proc == new_proc) {
        enable(m);
        return;
    }

    ctxsw(&old_proc->stk_ptr, &new_proc->stk_ptr);
    enable(m);
}
