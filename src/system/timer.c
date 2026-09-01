#include "uart.h"
#include "reg_util.h"
#include "timer.h"
#include "intr.h"

extern void handle_cpu_timer_intr(void);

void init_cpu_timer() {
    l1_interrupt_handlers[INUM_TIMER] = handle_cpu_timer_intr;
    WRITE_REG(0x3ff00014, READ_REG(0x3ff00014) & 0xFFFFFFE);
    asm("movi a12, 0\nwsr.ccount a12\nwsr.ccompare0 %0\nrsync" : : "r"(TICK_CYCLES) : "a12");
    intr_unmask(1u << INUM_TIMER);
}
