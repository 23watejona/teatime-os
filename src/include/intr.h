#ifndef INTR_H
#define INTR_H

#define INUM_GPIO 4
#define INUM_UART 5
#define INUM_TIMER 6

/* L1 sources every process runs with; INTENABLE is per process (ctxsw frame) */
#define PROC_INTENABLE ((1u << INUM_TIMER) | (1u << INUM_UART) | (1u << INUM_GPIO))

#define NUM_L1_INTR 14
extern void (*l1_interrupt_handlers[NUM_L1_INTR])(void);
unsigned int intr_unmask(unsigned int mask);
void _set_vec_base(void);

#endif
