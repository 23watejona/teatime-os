extern int initmem(void);
extern char *alloc_stack(unsigned int);
extern void start_proc(void (*)(void));


unsigned int *create(void *funcaddr, unsigned int stack_size) {
     // allocate stack and move the pointer to the beginning
    unsigned int *stack_addr = (unsigned int *)alloc_stack(stack_size) + ((stack_size >> 2));
    
    unsigned int *save_stack_addr; // we need this to keep track of where to store the stack pointer


    // Make it look like we were saved after a context switch
    
    // zero out ars a15-a3
    *stack_addr = 0;
    for (int i = 0; i < 13; ++i) {
        *--stack_addr = 0;
    }

    // a2 is where we pass the first argument. start_proc expects an argument of the process (function) to run
    *--stack_addr = (unsigned int)funcaddr; // set a2=argument 1 to be the function to run

    // a1 is the stack pointer`
    save_stack_addr = --stack_addr; // wait till later to store sp

    // a0 is the return address
    *--stack_addr = (unsigned int)start_proc; // wrapper to handle process ending
    
    // zero out special registers intenable, sar, ps, epc1, as each of these start in a fresh state
    *--stack_addr = 0; //interrupts are disabled
    *--stack_addr = 0; // sar should be 0
    *--stack_addr = 0; // ps is 0?
    *--stack_addr = 0; // epc1 is not set
    
    *save_stack_addr = (unsigned int)stack_addr;
    return stack_addr;
}
