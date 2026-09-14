#include "def.h"
#include "proc.h"
#include "proc_queue.h"


static struct {
    queue_entry *queue;
    volatile int pending;
    int used;
} condtab[NCOND];
static volatile int nmi_pending;

void cond_init(void) {
    for (int i = 0; i < NCOND; i++)
        condtab[i].queue = new_queue();
}

int cond_create(void) {
    int m = disable();
    for (int i = 0; i < NCOND; i++) {
        if (!condtab[i].used) {
            condtab[i].used = 1;
            condtab[i].pending = 0;
            enable(m);
            return i;
        }
    }
    enable(m);
    return -1;
}

IRAM_ATTR static void wake(int pid) {
    if (proctab[pid].status == PROC_TIMED_WAIT)
        sleep_remove(pid);
    make_avail(pid);
}

IRAM_ATTR static void wake_all(int c) {
    int pid;
    while ((pid = proc_dequeue(condtab[c].queue)) != NULL_PROC)
        wake(pid);
}

IRAM_ATTR static int wait(int c, int mutex, int timed, unsigned int delay) {
    int m = disable();
    // disable() doesn't mask the nmi, so it can signal before we queue (held
    // in pending) or after (the clock tick finds us on the queue)
    if (condtab[c].pending) {
        condtab[c].pending = 0;
        enable(m);
        return 0;
    }
    proctab[curr_pid].timed_out = 0;
    proctab[curr_pid].status = timed ? PROC_TIMED_WAIT : PROC_COND_WAIT;
    if (timed)
        sleep_enqueue(curr_pid, delay);
    proc_enqueue(condtab[c].queue, curr_pid, proctab[curr_pid].priority);
    // queue before unlocking, since the unlock can switch to the producer
    // and its signal would find no waiter
    if (mutex != MUTEX_NONE)
        mutex_unlock(mutex);
    if (proctab[curr_pid].status != PROC_CURR)
        sched();
    int rc = proctab[curr_pid].timed_out ? -1 : 0;
    enable(m);
    if (mutex != MUTEX_NONE)
        mutex_lock(mutex);
    return rc;
}

IRAM_ATTR void cond_wait(int c, int mutex) {
    wait(c, mutex, 0, 0);
}

IRAM_ATTR int cond_timedwait(int c, int mutex, unsigned int delay) {
    return wait(c, mutex, 1, delay);
}

IRAM_ATTR void cond_signal(int c) {
    int m = disable();
    int pid = proc_dequeue(condtab[c].queue);
    if (pid != NULL_PROC)
        wake(pid);
    if (need_resched)
        sched();
    enable(m);
}

IRAM_ATTR void cond_broadcast(int c) {
    int m = disable();
    wake_all(c);
    if (need_resched)
        sched();
    enable(m);
}

IRAM_ATTR void cond_signal_isr(int c) {
    int m = disable();
    if (proc_queue_empty(condtab[c].queue))
        condtab[c].pending = 1;
    else
        wake_all(c);
    enable(m);
}

IRAM_ATTR void cond_signal_nmi(int c) {
    // disable() doesn't mask the nmi, so we could be mid-way through a queue
    // update here. all we can do is set a flag for the clock tick
    condtab[c].pending = 1;
    nmi_pending = 1;
}

IRAM_ATTR void cond_clock(void) {
    if (!nmi_pending)
        return;
    nmi_pending = 0;
    for (int c = 0; c < NCOND; c++) {
        if (condtab[c].pending && !proc_queue_empty(condtab[c].queue)) {
            wake_all(c);
            condtab[c].pending = 0;
        }
    }
}
