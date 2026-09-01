#include "def.h"
#include "proc.h"
#include "proc_queue.h"


#define CLOCK_COND_PERIOD 100 /* ticks, ~100 ms */

static struct {
    queue_entry *queue;
    volatile int pending;
    int clocked;
    int used;
} condtab[NCOND];

static unsigned int clock_ticks;

int clock_cond;

static int new_cond(int clocked) {
    int m = disable();
    for (int i = 0; i < NCOND; i++) {
        if (!condtab[i].used) {
            condtab[i].used = 1;
            condtab[i].clocked = clocked;
            condtab[i].pending = 0;
            enable(m);
            return i;
        }
    }
    enable(m);
    return -1;
}

void cond_init(void) {
    for (int i = 0; i < NCOND; i++)
        condtab[i].queue = new_queue();
    clock_cond = cond_create_clocked();
}

int cond_create(void) {
    return new_cond(0);
}

int cond_create_clocked(void) {
    return new_cond(1);
}

IRAM_ATTR static void wake_all(int c) {
    int pid;
    while ((pid = proc_dequeue(condtab[c].queue)) != NULL_PROC)
        make_avail(pid);
}

void cond_wait(int c, int mutex) {
    int m = disable();
    if (condtab[c].pending) {
        condtab[c].pending = 0;
        enable(m);
        return;
    }
    if (mutex != MUTEX_NONE)
        mutex_unlock(mutex);
    proctab[curr_pid].status = PROC_COND_WAIT;
    proc_enqueue(condtab[c].queue, curr_pid, proctab[curr_pid].priority);
    sched();
    enable(m);
    if (mutex != MUTEX_NONE)
        mutex_lock(mutex);
}

IRAM_ATTR void cond_signal(int c) {
    int m = disable();
    int pid = proc_dequeue(condtab[c].queue);
    if (pid != NULL_PROC)
        make_avail(pid);
    enable(m);
}

IRAM_ATTR void cond_broadcast(int c) {
    int m = disable();
    wake_all(c);
    enable(m);
}

IRAM_ATTR void cond_signal_isr(int c) {
    int m = disable();
    int pid = proc_dequeue(condtab[c].queue);
    if (pid != NULL_PROC)
        make_avail(pid);
    else
        condtab[c].pending = 1;
    enable(m);
}

IRAM_ATTR void cond_signal_nmi(int c) {
    condtab[c].pending = 1;
}

void cond_clock(void) {
    int period = ++clock_ticks >= CLOCK_COND_PERIOD;
    if (period)
        clock_ticks = 0;
    for (int c = 0; c < NCOND; c++) {
        if (!condtab[c].used)
            continue;
        if (condtab[c].pending) {
            if (!proc_queue_empty(condtab[c].queue)) {
                wake_all(c);
                condtab[c].pending = 0;
            }
        } else if (period && condtab[c].clocked) {
            wake_all(c);
        }
    }
}
