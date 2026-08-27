#include "def.h"
#include "intr.h"
#include "proc.h"
#include "uart.h"
#include "reg_util.h"
#include "timer.h"
#include "wdt.h"

unsigned int intr_enable(unsigned int mask);

void (*l1_interrupt_handlers[NUM_L1_INTR])(void);

IRAM_ATTR void debug_handler() {
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

IRAM_ATTR void syscall_handler (unsigned int exccause, unsigned int *frame) {
    unsigned int interrupt = 0;
    unsigned int intenable = 0;
    switch (exccause) {
        default: {
            unsigned int epc, vaddr;
            asm("rsr.epc1 %0" : "=r"(epc));
            asm("rsr.excvaddr %0" : "=r"(vaddr));
            // interrupted a0 and sp, from the frame layout in ctxsw.s
            unsigned int a0 = frame[0x50 / 4];
            unsigned int a1 = (unsigned int)frame + 96;
            kprintf_uart("\nFATAL exccause=%d epc1=%x a0=%x a1=%x pid=%d "
                         "excvaddr=%x -- rebooting\n",
                         exccause, epc, a0, a1, curr_pid, vaddr);
            // rtc ram survives the reboot, so start() prints these as fatal_*
            WRITE_REG(0x60001214, epc);
            WRITE_REG(0x60001218, exccause);
            WRITE_REG(0x6000121c, a0);
            WRITE_REG(0x60001220, a1);
            system_reboot();
        }
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
            for (int i = 0; i < NUM_L1_INTR; ++i) {
                int curr_intr_mask = 1u << i;
                if (!(masked_interrupts & curr_intr_mask))
                    continue;
                asm("wsr.intclear %0\nrsync\n" : : "r"(curr_intr_mask) : );
                if (l1_interrupt_handlers[i])
                    l1_interrupt_handlers[i]();
            }
            break;
    }
}

IRAM_ATTR void double_exc_handler() {
  kprintf_uart("\nagggg what am i doing here\n");
}

void wifi_fiq_dispatch(void);

IRAM_ATTR void nmi_handler() {
    wifi_fiq_dispatch();
}

unsigned int intr_unmask(unsigned int mask) {
    // a level-triggered irq with no handler is never acked, so it storms the cpu
    for (int i = 0; i < NUM_L1_INTR; ++i) {
        if ((mask & (1u << i)) && !l1_interrupt_handlers[i]) {
            kprintf_uart("intr_unmask: INUM %d has no handler -- masked\n", i);
            mask &= ~(1u << i);
        }
    }
    return intr_enable(mask);
}
