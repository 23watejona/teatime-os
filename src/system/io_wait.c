#include "proc.h"
#include "proc_queue.h"

extern int curr_pid;
extern int disable(void);
extern void enable(int mask);
extern void sched(void);

static queue_entry *io_queue;
static volatile unsigned int io_pending;
static unsigned int clock_ticks;

#define IO_CLOCK_PERIOD 100 /* ticks, ~100 ms */

void io_wait_init(void) {
    io_queue = new_queue();
}

void io_wait(void) {
    int m = disable();
    proctab[curr_pid].status = PROC_IO_WAIT;
    proc_enqueue(io_queue, curr_pid, proctab[curr_pid].priority);
    sched();
    enable(m);
}

static void io_wake(void) {
    int m = disable();
    int pid;
    while ((pid = proc_dequeue(io_queue)) != NULL_PROC)
        make_avail(pid);
    enable(m);
}

void io_signal(void) {
    io_pending = 1;
}

void io_clock(void) {
    clock_ticks++;
    if (!io_pending && clock_ticks < IO_CLOCK_PERIOD)
        return;
    io_pending = 0;
    clock_ticks = 0;
    io_wake();
}
