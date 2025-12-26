#include "uart.h"
#include "reg_util.h"

void start ( void )
{
    _set_vec_base();
    BUSY_WAIT();
    initmem();
    alloc_stack(512);

    while(1)
    {
        BUSY_WAIT();
        kprintf_uart("Running main\n");
    }
}
