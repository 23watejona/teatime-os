#include "def.h"
#include "proc.h"
#include "proc_queue.h"


struct sleeper {
    unsigned int wake_at;
    struct sleeper *next;
};

static struct sleeper sleepers[NUM_PROC];
static struct sleeper *sleep_list;
static unsigned int tick_count;

IRAM_ATTR void sleep_enqueue(int pid, unsigned int delay) {
    struct sleeper *e = &sleepers[pid];
    e->wake_at = tick_count + delay;
    struct sleeper **p = &sleep_list;
    while (*p && (int)((*p)->wake_at - e->wake_at) <= 0)
        p = &(*p)->next;
    e->next = *p;
    *p = e;
}

IRAM_ATTR void sleep_remove(int pid) {
    struct sleeper *e = &sleepers[pid];
    for (struct sleeper **p = &sleep_list; *p; p = &(*p)->next) {
        if (*p == e) {
            *p = e->next;
            e->next = NULL;
            return;
        }
    }
}

IRAM_ATTR void sleep(unsigned int delay) {
    int m = disable();
    proctab[curr_pid].status = PROC_TIMED_WAIT;
    sleep_enqueue(curr_pid, delay);
    sched();
    enable(m);
}

unsigned int ticks(void) {
    return tick_count;
}

IRAM_ATTR void sleep_clock(void) {
    tick_count++;
    while (sleep_list && (int)(sleep_list->wake_at - tick_count) <= 0) {
        struct sleeper *e = sleep_list;
        sleep_list = e->next;
        e->next = NULL;
        int pid = e - sleepers;
        proc_remove(pid);
        proctab[pid].timed_out = 1;
        make_avail(pid);
    }
}
