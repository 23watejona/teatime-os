#ifndef PROC_H
#define PROC_H

#define PROC_UNUSED 0 // this proc is unused
#define PROC_UNAVAIL 1 // this proc is used, but cannot be scheduled
#define PROC_AVAIL 2 // this proc can be scheduled
#define PROC_CURR 3 // this proc is currently running
#define PROC_COND_WAIT 4 // this proc is blocked on a condition
#define PROC_SEM_WAIT 5 // this proc is blocked on a semaphore

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
} proctab_entry;

extern proctab_entry proctab[NUM_PROC];
extern int curr_pid;

/* kernel only */
int disable(void);
void enable(int mask);
void sched(void);

extern int create(void *func, unsigned int stack_size, int priority);

extern void make_avail(int pid);

#define NSEM 16
#define NCOND 16
#define MUTEX_NONE (-1)

void cond_init(void);
int cond_create(void);
/* also broadcast by the clock every CLOCK_COND_PERIOD ticks: for retries,
   give-up deadlines and pacing */
int cond_create_clocked(void);
extern int clock_cond; /* clocked, no other producer */
/* mutex (or MUTEX_NONE) is released while parked; re-check the predicate on return */
void cond_wait(int c, int mutex);
void cond_signal(int c);
void cond_broadcast(int c);
/* L1 handler side: wakes a waiter now, else remembered for the next wait */
void cond_signal_isr(int c);
/* NMI side: flag only; the clock delivers it on the next tick */
void cond_signal_nmi(int c);
void cond_clock(void);

void sem_init(void);
int sem_create(int initial);
void sem_wait(int s);
void sem_signal(int s);
int sem_count(int s);

int mutex_create(void);
void mutex_lock(int m);
void mutex_unlock(int m);

#endif // PROC_H
