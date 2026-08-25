#include "uart.h"

void main(void)
{
    while (1)
        asm("waiti 0");
}
