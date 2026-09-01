#include "uart.h"
#include "reg_util.h"
#include "timer.h"
#include "intr.h"

extern void handle_cpu_timer_intr(void);

void init_cpu_timer() {
    l1_interrupt_handlers[6] = handle_cpu_timer_intr;
    WRITE_REG(0x3ff00014, READ_REG(0x3ff00014) & 0xFFFFFFE);
    asm("movi a12, 0\nwsr.ccount a12\nwsr.ccompare0 %0\nrsync" : : "r"(TICK_CYCLES) : "a12");
    intr_unmask(1 << 6); // Enable CPU_CLK interrupts
    unsigned int intmask = 0;
    asm("rsr.intenable %r0" : "=r"(intmask));
}

void unused ( void )
{
    kprintf_uart("Enabling timer intr\n");
    /*frc1.ctrl.reload = 1;
    frc1.ctrl.enable = 1;
    frc1.ctrl.intr_type = 0;
    frc1.ctrl.div = 4;

    intr_unmask(1 << 9); // Enable FRC1 interrupts
    WRITE_REG_MASK(0x3ff00004, 0x00000002); // Enable timer interrupts
    
    kprintf_uart("Done enabling timer intr\n");
    kprintf_uart("Converting to us timer ticks\n");
    unsigned int timer_us = 1000000;
    unsigned int apb_clk_freq = 80 * 1000000;
    unsigned int timer_ticks = 0;
    if (timer_us > 0) {
        if (timer_us <= 0x35A) {
            timer_ticks = (timer_us * (apb_clk_freq >> 4)) / 1000000;
        } else {
            timer_ticks = (timer_us >> 2) * ((apb_clk_freq >> 4) / 250000);
            timer_ticks += (timer_us & 0x3) * ((apb_clk_freq >> 4) / 1000000);
        }
    }
    timer_ticks = (timer_ticks * 2) / 3;
    if (timer_ticks > 0x7FFFFF) {
        kprintf_uart("Too many ticks\n");
    }
    kprintf_uart("us: %d, Timer ticks: %d\n", timer_us, timer_ticks);
    kprintf_uart("Arming timer\n");

    frc1.load.data = timer_ticks;
    */
    
    //sched();
}
