#include "uart.h"
#include "proc.h"

extern int initmem(void);
extern char *alloc_stack(unsigned int);
extern void start_proc(void (*)(void));
extern int get_pid(void);


// since this is a stack frame we go in reverse order
#define NUM_AREG (16)
struct ctxsw_stack_frame {
    unsigned int epc1;
    unsigned int ps;
    unsigned int sar;
    unsigned int intenable;
    union {
        struct {
            unsigned int a0;
            unsigned int a1;
            unsigned int a2;
            unsigned int a3;
            unsigned int a4;
            unsigned int a5;
            unsigned int a6;
            unsigned int a7;
            unsigned int a8;
            unsigned int a9;
            unsigned int a10;
            unsigned int a11;
            unsigned int a12;
            unsigned int a13;
            unsigned int a14;
            unsigned int a15;
        } reg;
        unsigned int raw_areg_mem[NUM_AREG];
    } address_regs;
};

int create(void *funcaddr, unsigned int stack_size, int priority) {
    int pid = get_pid();
    if (pid < 0)
        return -1;

    char *stk = alloc_stack(stack_size);
    if (!stk) {
        proctab[pid].status = PROC_UNUSED;
        return -1;
    }

    // move the pointer to the top of the stack
    unsigned int *stack_addr = ((unsigned int *)stk) + (stack_size >> 2);
    
    // allocate space for stack frame
    stack_addr = (unsigned int *)((char *)(stack_addr) - sizeof(struct ctxsw_stack_frame));
    struct ctxsw_stack_frame *frame = (struct ctxsw_stack_frame *)stack_addr;
    
    // zero out aregs in frame
    for (int i = 0; i < NUM_AREG; ++i) {
        frame->address_regs.raw_areg_mem[i] = 0;
    }
    frame->address_regs.reg.a3 = (unsigned int)pid;
    frame->address_regs.reg.a2 = (unsigned int) funcaddr;
    frame->address_regs.reg.a1 = (unsigned int) stack_addr;
    frame->address_regs.reg.a0 = (unsigned int) start_proc;
    frame->intenable = 0x40; // bit 6 = clock interrupt
    frame->sar = 0;
    frame->ps = 0;
    frame->epc1 = (unsigned int)start_proc;

    proctab[pid].status = PROC_AVAIL;
    proctab[pid].stk_ptr = (unsigned int *)stack_addr;
    proctab[pid].priority = priority;
    
    return pid;
}
