#include "proc.h"
#include "def.h"
#include "wdt.h"
#include "reg_util.h"
#include "timer.h"

void cond_clock();
void sleep_clock();

volatile unsigned int clktime;
static unsigned int tick_in_sec;

IRAM_ATTR void handle_cpu_timer_intr(void) {
    wdt_feed();
    if (++tick_in_sec == TICKS_PER_SEC) {
        tick_in_sec = 0;
        clktime++;
    }
    asm("rsr.ccount a0\nadd a0, a0, %0\nwsr.ccompare0 a0\nrsync" : : "r"(TICK_CYCLES) : "a0");
    sleep_clock();
    cond_clock();
    // pulse the nmi arm gate so a still-pending event makes a fresh edge; never from the nmi itself, since that nests an entry inside the handler
    WRITE_REG(0x3ff00000, READ_REG(0x3ff00000) & 0xffffffe0);
    asm volatile("memw");
    WRITE_REG(0x3ff00000, (READ_REG(0x3ff00000) & 0xffffffe0) | 1);
    sched();
}
