#ifndef INTR_H
#define INTR_H

#define NUM_L1_INTR 14
extern void (*l1_interrupt_handlers[NUM_L1_INTR])(void);
unsigned int intr_unmask(unsigned int mask);

#endif
