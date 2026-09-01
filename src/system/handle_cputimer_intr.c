#include "proc.h"
#include "def.h"
#include "wdt.h"
#include "reg_util.h"
#include "timer.h"

void sched();
void io_clock();

volatile unsigned int clktime;
static unsigned int tick_in_sec;

IRAM_ATTR void handle_cpu_timer_intr(void) {
    wdt_feed();
    if (++tick_in_sec == TICKS_PER_SEC) {
        tick_in_sec = 0;
        clktime++;
    }
    asm("rsr.ccount a0\nadd a0, a0, %0\nwsr.ccompare0 a0\nrsync" : : "r"(TICK_CYCLES) : "a0");
    io_clock();
    /* pulse the arm gate so pending NMI events make a fresh edge.
       Never pulse from NMI context. */
    WRITE_REG(0x3ff00000, READ_REG(0x3ff00000) & 0xffffffe0);
    asm volatile("memw");
    WRITE_REG(0x3ff00000, (READ_REG(0x3ff00000) & 0xffffffe0) | 1);
    sched();
}
