#ifndef PROC_H
#define PROC_H

#define PROC_UNUSED 0
#define PROC_UNAVAIL 1 /* created, not yet made available */
#define PROC_AVAIL 2
#define PROC_CURR 3
#define PROC_COND_WAIT 4
#define PROC_SEM_WAIT 5
#define PROC_TIMED_WAIT 6 /* on the sleep list, with or without a cond */

#define NUM_PROC 10
#define NULL_PROC 0

#define PROC_NULL_PRIO 0 // strictly the lowest priority
#define PROC_MIN_PRIO 1
#define PROC_MAX_PRIO 15

/* stack-bottom canary, swept by sched() every tick */
#define STK_CANARY(pid) (0xC0FFEE00u ^ (unsigned int)(pid))

typedef struct proctab_entry {
    unsigned int status;
    unsigned int *stk_ptr;
    unsigned int *stk_base; /* stack bottom word, holds STK_CANARY(pid) */
    int priority;
    int timed_out; /* how the last timed wait ended */
} proctab_entry;

extern proctab_entry proctab[NUM_PROC];
extern int curr_pid;

/* kernel only */
int disable(void);
void enable(int mask);
void sched(void);
extern int need_resched;

extern int create(void *func, unsigned int stack_size, int priority);
/* create and make available; the pid, or -1 */
int spawn(void *func, unsigned int stack_size, int priority);
void ctxsw(unsigned int **old_sp, unsigned int **new_sp);

extern void make_avail(int pid);

#define NSEM 16
#define NCOND 16
#define MUTEX_NONE (-1)

void cond_init(void);
int cond_create(void);
/* mutex (or MUTEX_NONE) is released while parked; re-check the predicate on return */
void cond_wait(int c, int mutex);
/* cond_wait bounded by delay ticks: 0 signalled, -1 timed out */
int cond_timedwait(int c, int mutex, unsigned int delay);
void cond_signal(int c);
void cond_broadcast(int c);
/* L1 handler side: wakes every waiter now, else remembered for the next wait */
void cond_signal_isr(int c);
/* NMI side: flag only; the clock delivers it on the next tick */
void cond_signal_nmi(int c);
void cond_clock(void);

/* Parks the caller for delay ticks (1 ms each). */
void sleep(unsigned int delay);
void sleep_clock(void);
/* cond.c only, caller masked: put the current process on / off the sleep list */
void sleep_enqueue(int pid, unsigned int delay);
void sleep_remove(int pid);

void sem_init(void);
int sem_create(int initial);
void sem_wait(int s);
void sem_signal(int s);
int sem_count(int s);

int mutex_create(void);
void mutex_lock(int m);
void mutex_unlock(int m);

#endif // PROC_H
