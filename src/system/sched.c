#include "def.h"
#include "proc.h"
#include "uart.h"
#include "proc_queue.h"

extern int curr_pid;
extern void ctxsw(unsigned int **, unsigned int **);
extern queue_entry *avail_list;
extern void make_avail(int pid);
extern volatile unsigned int wifi_rx_pending;
extern int wifi_rx_servicer_pid;

void sched() {
    if (wifi_rx_pending && wifi_rx_servicer_pid >= 0 &&
        proctab[wifi_rx_servicer_pid].status == PROC_IO_WAIT) {
        wifi_rx_pending = 0;
        make_avail(wifi_rx_servicer_pid);
    }

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
        return;
    }

    ctxsw(&old_proc->stk_ptr, &new_proc->stk_ptr);
}
