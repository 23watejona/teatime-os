#ifndef POT_H
#define POT_H

#define POT_IDLE 0
#define POT_BREWING 1

struct tea {
    const char *uri;
    unsigned int steep_secs;
};

#define POT_TEAS 3
extern const struct tea pot_teas[POT_TEAS];

struct pot_status {
    int state;
    int tea;
    unsigned int elapsed_secs;
    int strength;
};

/* Opens the pump and solenoid outputs; call once before the rest. */
void pot_init(void);

/* Unsynchronized: call from one process only. Strength is a percentage of the
   variety's recommended steep, falling past 100 once it is overshot. */
int pot_start(int tea);
int pot_stop(void);
void pot_status(struct pot_status *st);

#endif
