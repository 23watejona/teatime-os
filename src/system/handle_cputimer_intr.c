#include "proc.h"
#include "def.h"
#include "wdt.h"
#include "reg_util.h"

void sched();

extern volatile unsigned int wifi_tick_pending;

void handle_cpu_timer_intr(void) {
    wdt_feed();
    asm("rsr.ccount a0\nadd a0, a0, %0\nwsr.ccompare0 a0\nrsync" : : "r"(800000) : "a0");
    wifi_tick_pending = 1;
    /* pulse the arm gate so pending NMI events make a fresh edge.
       Never pulse from NMI context. */
    WRITE_REG(0x3ff00000, READ_REG(0x3ff00000) & 0xffffffe0);
    asm volatile("memw");
    WRITE_REG(0x3ff00000, (READ_REG(0x3ff00000) & 0xffffffe0) | 1);
    sched();
}
