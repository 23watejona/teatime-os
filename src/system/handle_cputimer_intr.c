#include "proc.h"
#include "def.h"
#include "wdt.h"

void sched();

void handle_cpu_timer_intr(void) {
    wdt_feed();
    asm("rsr.ccount a0\nadd a0, a0, %0\nwsr.ccompare0 a0\nrsync" : : "r"(80000000) : "a0");
    sched();
}
