#include "uart.h"

extern void null_proc(void);


void start_proc(void (*func)(void)) {
    func();
    kprintf_uart("Process ended\n");
    null_proc(); // replace with call to scheduler
}
