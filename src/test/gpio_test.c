#include "dev.h"
#include "gpio.h"
#include "uart.h"

#define TEST_PIN 4

void gpio_test_proc(void) {
    int fd = open("gpio", TEST_PIN);
    if (fd < 0) {
        kprintf_uart("gpio: open failed\n");
        return;
    }
    unsigned char level;
    control(fd, GPIO_OUTPUT, 0);
    for (int i = 0; i < 2; i++) {
        level = i;
        write(fd, &level, 1);
        read(fd, &level, 1);
        kprintf_uart("gpio%d: drove %d, reads %d\n", TEST_PIN, i, level);
    }
    control(fd, GPIO_INPUT, GPIO_PULLUP);
    while (1) {
        read(fd, &level, 1);
        kprintf_uart("gpio%d: edge, now %d\n", TEST_PIN, level);
    }
}
