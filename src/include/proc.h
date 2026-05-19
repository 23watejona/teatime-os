#ifndef PROC_H
#define PROC_H

#define PROC_UNUSED 0 // this proc is unused
#define PROC_UNAVAIL 1 // this proc is used, but cannot be scheduled
#define PROC_AVAIL 2 // this proc can be scheduled
#define PROC_CURR 3 // this proc is currently running

#define NUM_PROC 10
#define NULL_PROC 0

#define PROC_NULL_PRIO 0 // strictly the lowest priority
#define PROC_MIN_PRIO 1
#define PROC_MAX_PRIO 15

typedef struct proctab_entry {
    unsigned int status;
    unsigned int *stk_ptr;
    int priority;
} proctab_entry;

extern proctab_entry proctab[NUM_PROC];
extern int curr_pid;

/* kernel only */
int disable(void);
void enable(int mask);
void sched(void);

extern int create(void *func, unsigned int stack_size, int priority);

#endif // PROC_H
