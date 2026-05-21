#include "reg_util.h"
#include "wait.h"
#include "rf_i2c.h"
#include "uart.h"

void wifi_set_channel(unsigned int ch);
void init_wifi_pbus(void);
void init_wifi_bb(void);

void rx_path_enable(void) {
    WRITE_REG_RMW(0x600005e8, 0xfe7fffff, 0);
    wait_us(5);
    WRITE_REG_RMW(0x60009a34, 0xfffffc00, 0xf1);
    WRITE_REG_RMW(0x60009a34, 0xfffffc00, 0xf0);
    wait_us(5);
}

void dpd_bypass(void) {
    rf_i2c_write_mask(119, 0, 15, 1, 1, 1);
}

void rc_calibrate(void) {
    kprintf_uart("      rc:w1\n");
    rf_i2c_write_mask(106, 2, 0, 5, 4, 0);
    rf_i2c_write_mask(106, 2, 4, 7, 4, 2);
    rf_i2c_write_mask(104, 3, 1, 0, 0, 1);
    rf_i2c_write_mask(106, 2, 6, 4, 0, 8);
    rf_i2c_write_mask(106, 2, 4, 0, 0, 1);
    rf_i2c_write_mask(106, 2, 3, 6, 6, 1);
    rf_i2c_write_mask(106, 2, 4, 3, 3, 0);
    rf_i2c_write_mask(106, 2, 4, 3, 3, 1);
    kprintf_uart("      rc:wait\n");
    wait_us(100);

    kprintf_uart("      rc:read\n");
    unsigned int v = rf_i2c_read(106, 2, 5) & 0x3f;
    rf_i2c_write_mask(104, 3, 1, 0, 0, 0);
    kprintf_uart("      rc:done\n");

    int rc_a = (signed char)((11 * (int)v - 14) / 20);
    int rc_b = (signed char)((((unsigned char)((unsigned short)(28 * v) / 9)) + 2) >> 2);

    rf_i2c_write(97,  1, 2, (rc_a | 0xa0) & 0xff);
    rf_i2c_write(102, 3, 1, (rc_b | 0x40) & 0xff);
}

static void sar_init(void) {
    WRITE_REG_MASK(0x60000710, 0x00000002);
    rf_i2c_write_mask(108, 2, 0, 4, 4, 1);
    rf_i2c_write_mask(108, 2, 1, 1, 0, 2);
}

static void tx_regs_init(void) {
    rf_i2c_write(107, 2, 1, 104);
    rf_i2c_write(107, 2, 2, 15);
    rf_i2c_write(107, 2, 3, 168);
    rf_i2c_write(107, 2, 4, 6);
    rf_i2c_write(107, 2, 5, 8);
    rf_i2c_write(107, 2, 6, 184);
    rf_i2c_write(107, 2, 7, 91);
    rf_i2c_write(107, 2, 8, 4);
    rf_i2c_write(107, 2, 9, 0);
    rf_i2c_write(107, 2, 10, 116);
    rf_i2c_write(107, 2, 11, 7);
}

void rf_regs_init(void) {
    rf_i2c_write(106, 2, 0, 37);
    tx_regs_init();
    rf_i2c_write(107, 2, 1, 72);
    rf_i2c_write_mask(100, 0, 5, 1, 0, 3);
    rf_i2c_write(97, 1, 8, 17);
    rf_i2c_write(108, 2, 0, 21);
    sar_init();
    rf_i2c_write_mask(103, 4, 4, 7, 7, 1);
    rf_i2c_write(106, 2, 0, 41);
    rf_i2c_write(98, 1, 4, 175);
    rf_i2c_write(98, 1, 11, 128);
    WRITE_REG_RMW(0x60000d40, 0xfffff9ff, 0x400);
}

void pbus_force(unsigned int a, unsigned int b, unsigned int c) {
    unsigned int v = (READ_REG(0x60000594) & 0xffff0001) | 2;
    v |= (a & 0xff) << 2;
    v |= (c & 0xffff) << 5;
    v |= (b & 0xff) << 14;
    WRITE_REG(0x60000594, v);
    for (unsigned int t = 0; (int)READ_REG(0x600005a0) < 0 && t < 100000u; t++)
        ;
    // only the force-test strobe is cleared; debug mode stays on until pbus_work_mode
    WRITE_REG_UNMASK(0x60000594, 2);
}

void pbus_tx_power_off(void) {
    pbus_force(6, 1, 0);
    pbus_force(1, 1, 12);
    pbus_force(2, 1, 128);
}

void pbus_work_mode(void) {
    /* clear bit0 -- the pbus debug-enable that pbus_debug_mode set. */
    WRITE_REG_UNMASK(0x60000594, 1);
    WRITE_REG_UNMASK(0x60009b08, 0x00000008);
}

void pbus_debug_mode(void) {
    /* assumes pbus idle at cold boot; busy-drain path not ported */
    WRITE_REG_MASK(0x60009b08, 0x00000008);
    WRITE_REG_MASK(0x60000594, 1);
}

static void rx_max_gain_analog(void) {
    int d = rf_i2c_read(98, 1, 5);

    int g0 = (24 * d - 340) / 227;
    if (g0 > 15) g0 = 15;
    if (g0 == -1) g0 = 0;

    int g1 = (23 * d + 29) / 207;
    if (g1 > 15) g1 = 15;

    rf_i2c_write(100, 0, 4, (g0 & 0xf) | 64);
    rf_i2c_write(100, 0, 7, (g1 & 0xf) | 64);
    rf_i2c_write(97, 1, 5, 224);
}

static void rx_analog_init(void) {
    rf_i2c_write(97, 1, 8, 17);
    rx_max_gain_analog();
}

void rf_init(void) {
    WRITE_REG_MASK(0x3ff00018, 0xffff0000);

    WRITE_REG(0x60000700, 0x0019c06a);
    WRITE_REG(0x60000710, 0xf0000000);
    WRITE_REG_UNMASK(0x600005e8, 0x01800000);
    WRITE_REG_UNMASK(0x600005e8, 0x00000008);
    WRITE_REG_MASK(0x60000710, 0x02000000);

    WRITE_REG_MASK(0x60000710, 0x30000000);
    WRITE_REG_RMW(0x600005a0, 0xe0ffffff, 0x1c000000);
    WRITE_REG_RMW(0x60000598, 0xcfffffff, 0x10000000);
    WRITE_REG_UNMASK(0x3ff20c70, 2);
    wait_us(2);
    WRITE_REG_MASK(0x60000710, 0x0c000000);

    pbus_debug_mode();
    pbus_force(2, 1, 129);
    pbus_tx_power_off();
    pbus_work_mode();

    rf_i2c_write(98, 1, 0, 40);
    wifi_set_channel(1);
    rx_analog_init();
    rf_regs_init();
    rf_i2c_write(119, 0, 26, 8);
    rf_i2c_write(119, 0, 26, 56);
}

static void tx_dc_offset_apply(unsigned int val16, const signed char *tbl) {
    unsigned int fld = (val16 & 0xffff) << 8;
    int c = tbl[1];
    int d = tbl[0];
    for (unsigned int i = 0; i < 24; i++) {
        volatile unsigned int *r1 = (volatile unsigned int *)0x60000504 + i;
        *r1 = (*r1 & 0xfff000ff) | fld;
        volatile unsigned int *r2 = (volatile unsigned int *)0x60000404 + (i >> 1);
        unsigned int a4;
        if (i & 1) {
            a4 = (unsigned int)(((int)(c << 24)) >> 3)  |
                 (unsigned int)(((int)(d << 24)) >> 10);
            *r2 = a4 | (*r2 & 0xf0003fff);
        } else {
            a4 = (unsigned int)(((int)(c << 24)) >> 17) |
                 (unsigned int)(((int)(d << 24)) >> 24);
            *r2 = a4 | (*r2 & 0xffffc000);
        }
    }
}

/* DC-offset measurement engine. */
#define MCAL_REG       0x60000d4c
#define MCAL_ARM       0x01113cf1
#define MCAL_TRIGGER   0x01113cf3
#define MCAL_STOP      0x01113cf0
#define MCAL_DONE      0x00000001
#define MCAL_IFLAG     0x00000040   /* bit6: I-axis servo direction */
#define DC_CAL_SAMPLES 12u
#define DC_CAL_AVG     4u
#define DC_POLL_CAP    100000u

static int dc_clamp_s8(int v) {
    if (v > 127) return 127;
    if (v < 0) return 0;
    return v;
}

/* one gain-step SAR DC measurement; binary-search step from 0x1c, I/Q init 0x40. */
static void tx_dc_offset_measure(unsigned int gain, signed char out[2]) {
    int acc_i = 0, acc_q = 0;
    int prev_i = 0x40, prev_q = 0x40;
    int step = 0x1c;

    pbus_force(0, 1, gain);

    for (unsigned int n = 0; n < DC_CAL_SAMPLES; n++) {
        pbus_force(1, 2, (unsigned int)prev_q & 0xffff);
        pbus_force(0, 2, (unsigned int)prev_i & 0xffff);

        WRITE_REG(MCAL_REG, MCAL_ARM);
        WRITE_REG(MCAL_REG, MCAL_TRIGGER);
        wait_us(2);

        for (unsigned int t = 0; t < DC_POLL_CAP; t++)
            if (READ_REG(MCAL_REG) & MCAL_DONE)
                break;

        unsigned int s = READ_REG(MCAL_REG);
        int st = (signed char)step;

        if ((int)s < 0) prev_q -= st;
        else            prev_q += st;
        prev_q = dc_clamp_s8(prev_q);

        if (!(s & MCAL_IFLAG)) prev_i += st;
        else                   prev_i -= st;
        prev_i = dc_clamp_s8(prev_i);

        step = (st == 2) ? 1 : (st >> 1) + 1;

        if (n >= DC_CAL_SAMPLES - DC_CAL_AVG) {
            acc_i = (short)(acc_i + prev_i);
            acc_q = (short)(acc_q + prev_q);
        }
    }

    acc_q = (acc_q + 2) >> 2;
    acc_i = (acc_i + 2) >> 2;

    pbus_force(1, 2, (unsigned int)acc_q & 0xffff);
    pbus_force(0, 2, (unsigned int)acc_i & 0xffff);
    pbus_force(1, 1, 127);
    WRITE_REG(MCAL_REG, MCAL_STOP);

    out[0] = (signed char)acc_i;
    out[1] = (signed char)acc_q;
}

/* writes live-measured I/Q codes. */
static void tx_dc_offset_calibrate(void) {
    static const unsigned char gain_tbl[4] = {0x04, 0x10, 0x12, 0x14};
    signed char dc[2];
    signed char applied[2];
    int sum_i = 0, sum_q = 0;

    pbus_debug_mode();
    pbus_force(1, 1, 31);

    for (unsigned int g = 0; g < 4; g++) {
        tx_dc_offset_measure(gain_tbl[g], dc);
        sum_i += dc[0];
        sum_q += dc[1];
    }

    applied[0] = (signed char)((sum_i + 2) >> 2);
    applied[1] = (signed char)((sum_q + 2) >> 2);
    tx_dc_offset_apply(0, applied);

    pbus_work_mode();
}

/* noise floor -> 0x60009b64, ctrl -> 0x60009b60. */
static void noise_floor_set(int nf) {
    WRITE_REG_UNMASK(0x60009b60, 2);
    int v = nf + 1;
    v = (v + (int)((unsigned int)v >> 31)) >> 1;
    WRITE_REG_RMW(0x60009b64, 0xfffffe00, (unsigned int)v & 0x1ff);
    WRITE_REG_RMW(0x60009b60, 0xfffd7ffd, 2);
}

static int noise_floor_get(void) {
    unsigned int r = READ_REG(0x60009b64);
    int v = (int)((r >> 20) & 0xfff);
    v = (v + 1) >> 1;
    return (short)(v - 0x800);
}

static int noise_floor_clamped(void) {
    int v = noise_floor_get();
    if (v >= -340) return -340;
    if (v <  -392) return -392;
    return v;
}

static void noise_floor_start(unsigned int ch3) {
    if (READ_REG(0x60009b60) & 2)
        return;
    WRITE_REG_RMW(0x60009b64, 0xfffff000, ((ch3 & 7) << 9) | 0x1a0);
    WRITE_REG_MASK(0x60009b60, 0x00028002);
}

static int noise_floor_measure(unsigned int chan) {
    unsigned int n;
    if (READ_REG(0x60009b60) & 2) {
        for (n = 0; n < 20000; n++) {
            if (!(READ_REG(0x60009b60) & 2))
                break;
            wait_us(1);
        }
        if (n >= 20000)
            return 1;
    }
    wait_us(2);
    noise_floor_start(chan);
    for (n = 0; n < 20000; n++) {
        if (!(READ_REG(0x60009b60) & 2))
            return 0;
        wait_us(1);
    }
    return 1;
}

static void noise_init(void) {
    noise_floor_set(-388);
    WRITE_REG_RMW(0x60009a34, 0xfffffc00, 201);
    WRITE_REG_UNMASK(0x60009a34, 1);

    int nf[4];
    nf[0] = nf[1] = nf[2] = nf[3] = -340;
    unsigned int s60 = READ_REG(0x60009b60);
    unsigned int s20 = READ_REG(0x60009d20);
    unsigned int s40 = READ_REG(0x60009d40);

    WRITE_REG_UNMASK(0x60009b60, 2);
    WRITE_REG_UNMASK(0x60009d40, 0x40000000);
    wifi_set_channel(1);

    int ok = 0;
    for (int g = 0; g < 4; g++) {
        if (noise_floor_measure(1)) {
            WRITE_REG_UNMASK(0x60009b60, 2);
            break;
        }
        ok = 1;
        int s = noise_floor_clamped();
        if (s < nf[g])
            nf[g] = s;
    }

    WRITE_REG(0x60009b60, s60);
    WRITE_REG(0x60009d20, s20);
    WRITE_REG(0x60009d40, s40);

    int mn = -388;
    if (ok) {
        mn = nf[0];
        for (int i = 1; i < 4; i++)
            if (nf[i] < mn) mn = nf[i];
    }
    noise_floor_set(mn);
}

void bb_bringup(void) {
    rf_i2c_write(97, 1, 7, 81);
    init_wifi_pbus();
    rf_i2c_write_mask(119, 0, 16, 0, 0, 1);
    kprintf_uart("    bb: rc_calibrate\n");
    rc_calibrate();
    kprintf_uart("    bb: dcoffset\n");
    tx_dc_offset_calibrate();
    kprintf_uart("    bb: init_wifi_bb\n");
    init_wifi_bb();
    WRITE_REG_MASK(0x3ff20c70, 2);
    kprintf_uart("    bb: noise_init\n");
    noise_init();
    kprintf_uart("    bb: dpd_bypass\n");
    dpd_bypass();
}

void antenna_switch_init(void) {
    /* single-antenna board: chip6_phy_init_ctrl[34]&~2==1 antenna map. */
    WRITE_REG(0x60009d60, 0x01010101);
    WRITE_REG(0x60009d64, 0x01010104);
    WRITE_REG_MASK(0x60009b00, 0x00800000);
    WRITE_REG_RMW(0x60009b08, 0xffffc3ff, 0x00000800);
    WRITE_REG(0x60009a28, 2);
}

void bbpll_calibrate(unsigned int slow) {
    unsigned int saved = READ_REG(0x3ff00014);
    WRITE_REG(0x3ff00014, saved & 0xfffffffe);
    wait_us(1);
    WRITE_REG_RMW(0x60000d40, 0xfffffff2, 8);
    wait_us(slow ? 1000 : 100);
    WRITE_REG_RMW(0x60000d40, 0xfffffff2, 4);
    wait_us(1);
    WRITE_REG(0x3ff00014, saved);
}

/* saves 0x60009a2c, masks digital RX off (bit19). */
unsigned int rx_digital_stop(void) {
    unsigned int saved = READ_REG(0x60009a2c);
    WRITE_REG_MASK(0x60009b08, 8);
    WRITE_REG_UNMASK(0x60009a2c, 0x00080000);
    return saved;
}

/* pulse 0x60009b60 bit0, restore saved 0x60009a2c. */
void rx_digital_start(unsigned int a2c_saved) {
    WRITE_REG_UNMASK(0x60009b08, 8);
    WRITE_REG_MASK(0x60009b60, 1);
    WRITE_REG_UNMASK(0x60009b60, 1);
    WRITE_REG(0x60009a2c, a2c_saved);
}

/* clear 0x60009b00 bit28 (enable AGC/CCA). */
void agc_enable(void) {
    WRITE_REG_UNMASK(0x60009b00, 0x10000000);
}

/* set 0x60009b00 bit28 (disable AGC/CCA). */
void agc_disable(void) {
    WRITE_REG_MASK(0x60009b00, 0x10000000);
}

/* BB RX clock via RF-I2C 119/28 bit5, 124/21 bit1. */
void rx_clock_enable(unsigned int en) {
    rf_i2c_write_mask(119, 0, 28, 5, 5, en);
    rf_i2c_write_mask(124, 1, 21, 1, 1, en);
}

/* force powerup option 3 (full RF cal) in 0x6000073c. */
static void powerup_option_set(void) {
    WRITE_REG(0x6000073c, 3);
}

void init_wifi_rf(void) {
    kprintf_uart("  rf: powerup_option_set\n");
    powerup_option_set();
    kprintf_uart("  rf: rf_init\n");
    rf_init();
    kprintf_uart("  rf: bb_bringup\n");
    bb_bringup();
    kprintf_uart("  rf: antenna_switch_init\n");
    antenna_switch_init();
    /* BBPLL freq config: block 103 reg4 [4:0]=0x13. */
    rf_i2c_write_mask(103, 4, 4, 4, 0, 0x13);
    kprintf_uart("  rf: bbpll_calibrate\n");
    bbpll_calibrate(0);
    kprintf_uart("  rf: rx_clock_enable\n");
    rx_clock_enable(1);
    kprintf_uart("  rf: rx_path_enable\n");
    rx_path_enable();
    kprintf_uart("  rf: rx_digital_start\n");
    rx_digital_start(rx_digital_stop());
    kprintf_uart("  rf: agc_enable\n");
    agc_enable();

    /* arm the BB noise-floor machine; steady-state RX keeps it active. */
    WRITE_REG_UNMASK(0x60009b60, 2);
    noise_floor_start(1);
}
