#include "uart.h"

int s = 0;
void notmain ( void )
{
    __asm__ __volatile__(
        "movi       a0, 0x40100000\n"
        "wsr        a0, vecbase\n"
        : : :"memory");
    while(1)
    {
        for(int i=0;i<10000000;i++);
        kprintf_uart("\nTesting %d\n", 100);
        kprintf_uart("About to break narrow\n");
        asm("break.n 0");
        kprintf_uart("About to break regular\n");
        asm("break 0,0");
        kprintf_uart("Successfully here\n");
        asm("syscall");
        kprintf_uart("just syscalled");
    }
}

void print_stuff() {
  if (s == 0) {
    kprintf_uart("testing testing testing\n");
    s = 1;
  }
}
