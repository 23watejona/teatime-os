#include "uart.h"
#include "proc.h"

extern int initmem(void);
extern char *alloc_stack(unsigned int);
extern void start_proc(void (*)(void));
extern int get_pid(void);


// since this is a stack frame we go in reverse order
#define NUM_AREG (16)
struct ctxsw_stack_frame {
    unsigned int epc1; // a0
    unsigned int ps; // a0
    unsigned int sar; // a0
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


void verify_size() {
    int dummy = 0;
    switch (dummy) {
        case 0 == 1:
        case sizeof(struct ctxsw_stack_frame) == 80: // Stack frame should be 80 bytes
            break;
    }
}

int create(void *funcaddr, unsigned int stack_size, int priority) {
    int pid = get_pid();
    
    // allocate stack and move the pointer to the beginning
    unsigned int *stack_addr = ((unsigned int *)alloc_stack(stack_size)) + (stack_size >> 2);
    
    // allocate space for stack frame
    stack_addr = (unsigned int *)((char *)(stack_addr) - sizeof(struct ctxsw_stack_frame));
    struct ctxsw_stack_frame *frame = (struct ctxsw_stack_frame *)stack_addr;
    
    // zero out aregs in frame
    for (int i = 0; i < NUM_AREG; ++i) {
        frame->address_regs.raw_areg_mem[i] = 0;
    }
    frame->address_regs.reg.a3 = pid;
    frame->address_regs.reg.a2 = (unsigned int) funcaddr;
    frame->address_regs.reg.a1 = (unsigned int) stack_addr;
    frame->address_regs.reg.a0 = (unsigned int) start_proc;
    frame->intenable = 0;
    frame->sar = 0;
    frame->ps = 0;
    frame->epc1 = 0;

    proctab[pid].status = PROC_AVAIL;
    proctab[pid].stk_ptr = (unsigned int *)stack_addr;
    proctab[pid].priority = priority;
    
    return pid;
}
