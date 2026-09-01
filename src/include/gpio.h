#ifndef GPIO_H
#define GPIO_H

/* A "gpio" descriptor is opened with the pin number as arg and starts as a
   floating input. write: each byte drives the level, 0 low. read: one byte,
   an output's level at once, an input's level after the next edge. */
#define GPIO_OUTPUT 1
#define GPIO_INPUT 2 /* arg: GPIO_PULLUP or 0 */
#define GPIO_PULLUP 1

struct gpio_ctrl {
    unsigned int out;
    unsigned int out_set;
    unsigned int out_clear;
    unsigned int enable;
    unsigned int enable_set;
    unsigned int enable_clear;
    unsigned int in;
    unsigned int status;
    unsigned int status_set;
    unsigned int status_clear;
    unsigned int pin[16];
};

extern volatile struct gpio_ctrl gpio;

void gpio_init(void);

#endif
