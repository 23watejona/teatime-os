#ifndef TIMER_H
#define TIMER_H

static inline unsigned int ccount(void) {
    unsigned int c;
    asm volatile("rsr.ccount %0" : "=r"(c));
    return c;
}

#define TICK_CYCLES 80000 /* 1 ms at 80 MHz */
#define TICKS_PER_SEC 1000

/* seconds since boot */
extern volatile unsigned int clktime;

struct frc1_t {
    struct {
        unsigned int data: 23;
        unsigned int reserved: 9;
    } load;
    
    struct {
        unsigned int data: 23;
        unsigned int reserved: 9;
    } count;

    struct {
        unsigned int div: 6;
        unsigned int reload: 1;
        unsigned int enable: 1;
        unsigned int intr_type: 1;
        unsigned int reserved: 23;
    } ctrl;

    struct {
        unsigned int clear: 1;
        unsigned int reserved: 31;
    } intr;
};

extern volatile struct frc1_t frc1;

_Static_assert(sizeof(struct frc1_t) == 16, "sizeof(struct frc1_t) != 16");

struct rtc_t {
    unsigned int _res00;

    unsigned int slp_val;

    unsigned int analog_0;

    unsigned int pwr;

    unsigned int _res10;

    struct {
        unsigned int cause: 4;
        unsigned int reserved: 28;
    } state1;

    struct {
        unsigned int reserved_lo: 8;
        unsigned int wakeup_cause: 6;
        unsigned int reserved_hi: 18;
    } state2;

    unsigned int slp_cnt_val;

    unsigned int sleep_state;

    unsigned int sleep_mask;

    unsigned int status;

    unsigned int _res2c;

    unsigned int scratch[4];

    unsigned int _res40[2];

    unsigned int timing[2];

    unsigned int _res50[2];

    unsigned int analog_1;

    unsigned int analog_2;

    unsigned int analog_3;

    unsigned int _res64;

    unsigned int gpio_out;

    unsigned int _res6c[2];

    unsigned int gpio_enable;

    unsigned int _res78[2];

    unsigned int analog_6;

    unsigned int _res84[2];

    struct {
        unsigned int level: 1;
        unsigned int reserved: 31;
    } gpio_in_data;

    unsigned int gpio_conf;

    unsigned int clk_1;

    unsigned int clk_2;

    unsigned int clk_3;

    unsigned int pad_xpd_dcdc_conf;

    unsigned int _resa4[2];

    unsigned int trim[3];

    unsigned int _resb8[18];
};

extern volatile struct rtc_t rtc;

_Static_assert(sizeof(struct rtc_t) == 256, "sizeof(struct rtc_t) != 256");

#endif // TIMER_H
