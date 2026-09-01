#include "timer.h"
#include "pot.h"

const struct tea pot_teas[POT_TEAS] = {
    { "/darjeeling", 180 },
    { "/earl-grey", 240 },
    { "/peppermint", 300 },
};

static int state = POT_IDLE;
static int brewing;
static unsigned int started;

int pot_start(int tea) {
    if (state != POT_IDLE)
        return -1;
    state = POT_BREWING;
    brewing = tea;
    started = clktime;
    return 0;
}

int pot_stop(void) {
    if (state != POT_BREWING)
        return -1;
    state = POT_IDLE;
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
