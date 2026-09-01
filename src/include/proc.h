#ifndef PROC_H
#define PROC_H

#define PROC_UNUSED 0 // this proc is unused
#define PROC_UNAVAIL 1 // this proc is used, but cannot be scheduled
#define PROC_AVAIL 2 // this proc can be scheduled
#define PROC_CURR 3 // this proc is currently running
#define PROC_IO_WAIT 4 // this proc is blocked waiting on IO
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

/* must be called with interrupts off */
extern void make_avail(int pid);

void io_wait_init(void);
/* may be spurious, re-check the condition on wake */
extern void io_wait(void);
/* NMI-safe: only sets a flag; waiters wake at the next clock tick */
extern void io_signal(void);
extern void io_clock(void);

#define NSEM 8

void sem_init(void);
int sem_create(int initial);
void sem_wait(int s);
void sem_signal(int s);
int sem_count(int s);

int mutex_create(void);
void mutex_lock(int m);
void mutex_unlock(int m);

#endif // PROC_H
