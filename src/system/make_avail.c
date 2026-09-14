#include "def.h"
#include "proc.h"
#include "proc_queue.h"
#include "uart.h"
queue_entry *avail_list;
int need_resched;

IRAM_ATTR void make_avail(int pid) {
    int m = disable();
    proctab[pid].status = PROC_AVAIL;
    proc_enqueue(avail_list, pid, proctab[pid].priority);
    if (proctab[pid].priority > proctab[curr_pid].priority)
        need_resched = 1;
    enable(m);
}
