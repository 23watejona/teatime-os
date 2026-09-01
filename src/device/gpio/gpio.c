#include "def.h"
#include "gpio.h"
#include "dev.h"
#include "proc.h"
#include "intr.h"
#include "reg_util.h"

#define GPIO_LINES 4
#define GPIO_PIN_MAX 15

#define IOMUX_BASE 0x60000800
#define IOMUX_FUNC_BITS 0x130
#define IOMUX_PULLUP (1u << 7)

#define PIN_INT_ANY_EDGE (3u << 7)

struct pad {
    unsigned char offset;
    unsigned char gpio_func;
};

// pads 6-11 are the flash bus and gpio16 lives in the rtc block, so neither is offered
static const struct pad pads[GPIO_PIN_MAX + 1] = {
    [0] = { 0x34, 0 },
    [1] = { 0x18, 3 },
    [2] = { 0x38, 0 },
    [3] = { 0x14, 3 },
    [4] = { 0x3c, 0 },
    [5] = { 0x40, 0 },
    [12] = { 0x04, 3 },
    [13] = { 0x08, 3 },
    [14] = { 0x0c, 3 },
    [15] = { 0x10, 3 },
};

struct gpio_line {
    int pin;
    int input;
    volatile int edge;
    volatile unsigned char level;
    struct dev *dev;
};

static struct gpio_line lines[GPIO_LINES];

static unsigned int func_bits(unsigned int func) {
    return ((func & 3) << 4) | ((func >> 2) << 8);
}

static unsigned int level_of(int pin) {
    return (gpio.in >> pin) & 1;
}

static int gpio_open(struct dev *d, int arg) {
    struct gpio_line *l = d->state;
    if (arg < 0 || arg > GPIO_PIN_MAX || !pads[arg].offset)
        return -1;
    for (int i = 0; i < GPIO_LINES; i++)
        if (&lines[i] != l && lines[i].dev->used && lines[i].pin == arg)
            return -1;
    l->pin = arg;
    l->input = 1;
    l->edge = 0;
    WRITE_REG_RMW(IOMUX_BASE + pads[arg].offset, ~(IOMUX_FUNC_BITS | IOMUX_PULLUP),
                  func_bits(pads[arg].gpio_func));
    gpio.pin[arg] = 0;
    gpio.enable_clear = 1u << arg;
    return 0;
}

static int gpio_close(struct dev *d) {
    struct gpio_line *l = d->state;
    gpio.pin[l->pin] = 0;
    gpio.enable_clear = 1u << l->pin;
    return 0;
}

static int gpio_control(struct dev *d, int op, int arg) {
    struct gpio_line *l = d->state;
    unsigned int bit = 1u << l->pin;
    unsigned int pad = IOMUX_BASE + pads[l->pin].offset;
    switch (op) {
        case GPIO_OUTPUT:
            gpio.pin[l->pin] = 0;
            gpio.enable_set = bit;
            l->input = 0;
            return 0;
        case GPIO_INPUT:
            gpio.enable_clear = bit;
            WRITE_REG_RMW(pad, ~IOMUX_PULLUP, arg == GPIO_PULLUP ? IOMUX_PULLUP : 0);
            l->level = level_of(l->pin);
            l->edge = 0;
            l->input = 1;
            gpio.status_clear = bit;
            gpio.pin[l->pin] = PIN_INT_ANY_EDGE;
            return 0;
    }
    return -1;
}

static int gpio_write(struct dev *d, const void *buf, unsigned int n) {
    struct gpio_line *l = d->state;
    const unsigned char *in = buf;
    if (l->input)
        return -1;
    for (unsigned int i = 0; i < n; i++) {
        if (in[i])
            gpio.out_set = 1u << l->pin;
        else
            gpio.out_clear = 1u << l->pin;
    }
    return n;
}

static int gpio_read(struct dev *d, void *buf, unsigned int n) {
    struct gpio_line *l = d->state;
    unsigned char *out = buf;
    if (!l->input) {
        out[0] = level_of(l->pin);
        return 1;
    }
    while (!l->edge)
        cond_wait(d->cond, MUTEX_NONE);
    l->edge = 0;
    out[0] = l->level;
    return 1;
}

static void gpio_intr(void) {
    unsigned int status = gpio.status;
    gpio.status_clear = status;
    for (int i = 0; i < GPIO_LINES; i++) {
        struct gpio_line *l = &lines[i];
        if (!l->dev->used || !l->input || !(status & (1u << l->pin)))
            continue;
        l->level = level_of(l->pin);
        l->edge = 1;
        cond_signal_isr(l->dev->cond);
    }
}

static const struct dev_ops gpio_ops = {
    .open = gpio_open,
    .close = gpio_close,
    .read = gpio_read,
    .write = gpio_write,
    .control = gpio_control,
};

void gpio_init(void) {
    for (int i = 0; i < GPIO_LINES; i++)
        lines[i].dev = dev_register("gpio", &gpio_ops, &lines[i]);
    gpio.status_clear = 0xffff;
    l1_interrupt_handlers[INUM_GPIO] = gpio_intr;
    intr_unmask(1u << INUM_GPIO);
}
