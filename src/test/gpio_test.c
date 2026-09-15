#include "dev.h"
#include "gpio.h"
#include "uart.h"
#include "proc.h"
#include "timer.h"

#define PUMP_PIN 5
#define SOLENOID_PIN 4
#define SOLENOID_TICKS (1 * TICKS_PER_SEC)
#define PUMP_TICKS (3 * TICKS_PER_SEC)

static int load_open(int pin) {
    int fd = open("gpio", pin);
    if (fd < 0) {
        kprintf_uart("gpio%d: open failed\n", pin);
        return -1;
    }
    control(fd, GPIO_OUTPUT, 0);
    return fd;
}

static void load_set(int fd, int pin, unsigned char level) {
    unsigned char back;
    write(fd, &level, 1);
    read(fd, &back, 1);
    kprintf_uart("gpio%d: drove %d, reads %d\n", pin, level, back);
}

void gpio_test_proc(void) {
    int solenoid = load_open(SOLENOID_PIN);
    int pump = load_open(PUMP_PIN);
    if (solenoid < 0 || pump < 0)
        return;
    load_set(solenoid, SOLENOID_PIN, 1);
    sleep(SOLENOID_TICKS);
    load_set(solenoid, SOLENOID_PIN, 0);
    load_set(pump, PUMP_PIN, 1);
    sleep(PUMP_TICKS);
    load_set(pump, PUMP_PIN, 0);
    kprintf_uart("gpio: done\n");
}
