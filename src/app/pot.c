#include "timer.h"
#include "pot.h"
#include "dev.h"
#include "gpio.h"

const struct tea pot_teas[POT_TEAS] = {
    { "/darjeeling", 180 },
    { "/earl-grey", 240 },
    { "/peppermint", 300 },
};

#define PUMP_PIN 5
#define SOLENOID_PIN 4
#define LOAD_ON 1 // the MOSFET modules switch on high
#define LOAD_OFF 0

static int state = POT_IDLE;
static int brewing;
static unsigned int started;
static int pump = -1;
static int solenoid = -1;

static int load_open(int pin) {
    int fd = open("gpio", pin);
    if (fd >= 0)
        control(fd, GPIO_OUTPUT, 0);
    return fd;
}

static void load_set(int fd, unsigned char level) {
    if (fd >= 0)
        write(fd, &level, 1);
}

static void loads_set(unsigned char level) {
    load_set(solenoid, level);
    load_set(pump, level);
}

void pot_init(void) {
    pump = load_open(PUMP_PIN);
    solenoid = load_open(SOLENOID_PIN);
    loads_set(LOAD_OFF);
}

int pot_start(int tea) {
    if (state != POT_IDLE)
        return -1;
    state = POT_BREWING;
    brewing = tea;
    started = clktime;
    loads_set(LOAD_ON);
    return 0;
}

int pot_stop(void) {
    if (state != POT_BREWING)
        return -1;
    state = POT_IDLE;
    loads_set(LOAD_OFF);
    return 0;
}

void pot_status(struct pot_status *st) {
    st->state = state;
    st->tea = brewing;
    st->elapsed_secs = 0;
    st->strength = 0;
    if (state != POT_BREWING)
        return;
    unsigned int elapsed = clktime - started;
    unsigned int steep = pot_teas[brewing].steep_secs;
    st->elapsed_secs = elapsed;
    if (elapsed <= steep)
        st->strength = (int) (100 * elapsed / steep);
    else
        st->strength = 100 - (int) (100 * (elapsed - steep) / steep);
}
