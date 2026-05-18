#include "uart.h"
#include "reg_util.h"
#include "timer.h"

unsigned int intr_enable(unsigned int mask);
unsigned int disable();
unsigned int enable();
void sched();
void double_exc_handler();
void handle_cpu_timer_intr();

void (*l1_interrupt_handlers[12])(void) = {
    double_exc_handler, // 0 
    double_exc_handler, // 1
    double_exc_handler, // 2
    double_exc_handler, // 3
    double_exc_handler, // 4
    double_exc_handler, // 5
    handle_cpu_timer_intr, // 6 = cpu timer
    double_exc_handler, // 7
    double_exc_handler, // 8
    double_exc_handler, // 9
    double_exc_handler, // 10
    double_exc_handler, // 11
};

void debug_handler() {
  unsigned int debug_cause = 0;

  asm("rsr.debugcause %0": "=a" (debug_cause): : "memory");

  kprintf_uart("\nDebug Cause: %d\n", debug_cause);
  if (((debug_cause >> 3) & 0x1) == 1) {
    kprintf_uart("\nBREAK\n");
    asm("rsr a2, EPC2\n addi a2,a2,3\nwsr a2, EPC2\n" : : : "a2");
  } else {
    asm("rsr a2, EPC2\n addi a2,a2,2 \n wsr a2, EPC2\n" : : : "a2");
    kprintf_uart("\nBREAK.N\n");
  }
}

void syscall_handler (unsigned int exccause, unsigned int int_cause) {
    unsigned int interrupt = 0;
    unsigned int intenable = 0;
    switch (exccause) {
        case 0:
            kprintf_uart("Illegal Instruction\n");
            break;
        case 1:
            kprintf_uart("\nSyscall\n");
            kprintf_uart("Count: %d, intr: %d\n", frc1.count.data, frc1.intr.clear);
            __asm__ __volatile__("rsr a3, EPC1\n addi a3, a3, 3\n wsr a3, epc1" : : : "a3");
            break;
        case 28:
            kprintf_uart("\nIllegal Load\n");
            __asm__ __volatile__("rsr a3, EPC1\n addi a3, a3, 2\n wsr a3, epc1" : : : "a3");
            break;
        case 29:
            kprintf_uart("\nIllegal Store\n");
            __asm__ __volatile__("rsr a3, EPC1\n addi a3, a3, 2\n wsr a3, epc1" : : : "a3");
            break;
        
        // a level-1 interrupt, not a synchronous exception
        case 4:
            asm("rsr.interrupt %0\n" : "=r"(interrupt));
            asm("rsr.intenable %0\n" : "=r"(intenable));
            unsigned int masked_interrupts = interrupt & intenable;
            for (int i = 0; i < 12; ++i) {
                int curr_intr_mask = 1u << i;
                if ( masked_interrupts & curr_intr_mask) {
                    asm("wsr.intclear %0\nrsync\n" : : "r"(curr_intr_mask) : );
                    (l1_interrupt_handlers[i])();
               }
            }
            break;
    }
}

void double_exc_handler() {
  kprintf_uart("\nagggg what am i doing here\n");
}

unsigned int intr_unmask(unsigned int mask) {
    return intr_enable(mask);    
}
