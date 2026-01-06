#include "uart.h"
#include "reg_util.h"
#include "proc.h"
#include "proc_queue.h"

#define NULL_STK 512
#define INIT_STK 1024

extern void main(void);

extern void ctxsw(unsigned int **, unsigned int **);
extern int initmem(void);
extern char *alloc_stack(unsigned int);
extern void _set_vec_base(void);
extern void sched(void);
extern void make_avail(int);
extern queue_entry *avail_list;

proctab_entry proctab[NUM_PROC] = {0};
int curr_pid = 0;


void start ( void )
{
    _set_vec_base();
    BUSY_WAIT();
    initmem();

    alloc_stack(NULL_STK);
    
    for (int i = 0; i < NUM_PROC; ++i) {
        proctab[i].status = PROC_UNUSED;
    }

    /* set up null process entry */
    proctab_entry *null_pr = &(proctab[NULL_PROC]);
    null_pr->status = PROC_CURR;
    null_pr->stk_ptr = 0; // this will get overwritten with the correct sp the first time it is switched out
    null_pr->priority = PROC_NULL_PRIO; // null process has strictly the least priority
    
    curr_pid = 0; // currently running the "null" process

    avail_list = new_queue();

    make_avail(create(main, INIT_STK, 5));
    sched();
    
    // become the null process
    while (1) {
        kprintf_uart("halting processor from null proc\n");
        __asm__("waiti 0");
    }
}
