#include "uart.h"

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

void syscall_handler () {
  kprintf_uart("\nhandling syscall\n");
}

void double_exc_handler() {
  kprintf_uart("\nagggg what am i doing here\n");
}
