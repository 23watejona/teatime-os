#include "def.h"
#include "proc.h"
#include "proc_queue.h"
#include "uart.h"
queue_entry *avail_list;

void create_intr_frame();
void restore_intr_frame();

void make_avail(int pid) {
    proctab[pid].status = PROC_AVAIL;
    proc_enqueue(avail_list, pid, proctab[pid].priority);
    return;
}
