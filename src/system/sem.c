#include "proc.h"
#include "proc_queue.h"


static struct {
    int count;
    queue_entry *queue;
    int used;
} semtab[NSEM];

void sem_init(void) {
    for (int i = 0; i < NSEM; i++)
        semtab[i].queue = new_queue();
}

int sem_create(int initial) {
    int m = disable();
    for (int i = 0; i < NSEM; i++) {
        if (!semtab[i].used) {
            semtab[i].count = initial;
            semtab[i].used = 1;
            enable(m);
            return i;
        }
    }
    enable(m);
    return -1;
}

void sem_wait(int s) {
    int m = disable();
    if (--semtab[s].count < 0) {
        proctab[curr_pid].status = PROC_SEM_WAIT;
        proc_enqueue(semtab[s].queue, curr_pid, proctab[curr_pid].priority);
        sched();
    }
    enable(m);
}

void sem_signal(int s) {
    int m = disable();
    if (++semtab[s].count <= 0) {
        int pid = proc_dequeue(semtab[s].queue);
        make_avail(pid);
    }
    enable(m);
}

int sem_count(int s) {
    return semtab[s].count;
}

int mutex_create(void) {
    return sem_create(1);
}

void mutex_lock(int m) {
    sem_wait(m);
}

void mutex_unlock(int m) {
    if (sem_count(m) < 1)
        sem_signal(m);
}
