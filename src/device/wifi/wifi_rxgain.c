#include "reg_util.h"
#include "rf_i2c.h"
#include "wait.h"
#include "uart.h"
#include "wifi_rxgain.h"

extern void pbus_debug_mode(void);
extern void pbus_work_mode(void);
extern void pbus_force(unsigned int a, unsigned int b, unsigned int c);
extern void pbus_tx_power_off(void);
extern void agc_disable(void);

unsigned int pbus_rd(unsigned int reg, unsigned int width) {
    static const unsigned char base_off[8] =
        { 0x00, 0x02, 0x04, 0x05, 0x07, 0x09, 0x0b, 0x0c };
    reg &= 0xff;
    width &= 0xff;
    unsigned int w = (width == 4u) ? 2u : ((width - 1u) & 0xff);
    unsigned int slot = (w + base_off[reg & 7]) & 0xff;
    unsigned int word = READ_REG(0x600005a4 + (slot / 3u) * 4u);
    unsigned int shift = (slot == 12u) ? 0u : ((slot % 3u) * (unsigned int)(-9) + 0x12u);
    return (word >> (shift & 0x1f)) & 0x1ff;
}

static void pbus_set_rxgain(unsigned int g) {
    unsigned int p1 = ((g >> 3) & 1) << 6 | ((g >> 4) & 1) << 5 | ((g >> 9) & 1) |
                      ((g >> 5) & 1) << 4 | ((g >> 6) & 1) << 3 | ((g >> 7) & 1) << 2 |
                      ((g >> 8) & 1) << 1;
    unsigned int v1 = pbus_rd(3, 2);
    unsigned int p2 = ((g & 7) << 3) | (v1 & 0x1c7);
    unsigned int v2 = pbus_rd(2, 1);
    unsigned int p3 = (((g >> 0xb) & 0xf) << 3) | (((g >> 0xa) & 1) << 1) | (v2 & 0x185);
    pbus_force(3, 1, p1);
    pbus_force(3, 2, p2 & 0xffff);
    pbus_force(2, 1, p3 & 0xffff);
}

static const unsigned short bb_step_tab[8] =
    { 0x0000, 0x0010, 0x0014, 0x0015, 0x0017, 0, 0, 0 };

static unsigned int rx_gain_table_build(int *dc_table, unsigned int max_gain,
                                      const unsigned char *gain_idx,
                                      const unsigned char *bb_idx,
                                      unsigned int bb_len) {
    max_gain &= 0xff;
    bb_len &= 0xff;
    int last = (int)bb_len - 1;

    unsigned int seq = 0;
    unsigned int gain = 0;
    unsigned int pos = 0;

    while (1) {
        if (gain == (unsigned int)(int)(signed char)bb_idx[pos] && (int)pos < last) {
            do {
                pos = (pos + 1) & 0xff;
                if (bb_idx[pos] != 0)
                    break;
            } while ((int)pos < last);
            gain = 0;
        }

        int frac = (int)gain % 6;
        int step = (int)gain / 6;
        int bb = (bb_step_tab[step & 7] & 0x1fff) * 8;
        unsigned int val = (unsigned int)gain_idx[pos] * 0x100 + bb + frac;
        unsigned int half = val & 0xffff;

        int idx = (int)((seq & 0xff) << 24) >> 25;
        if ((seq & 1) == 0)
            dc_table[idx] = (int)half;
        else
            dc_table[idx] += (int)(val * 0x10000);

        if (max_gain < gain)
            return seq & 0xff;

        seq++;
        gain = (gain + 1) & 0xff;
        if (seq == 0x7f)
            return 0x55;
    }
}

#define IQ_EST_CTRL 0x6000057c
#define IQ_EST_I    0x600005dc
#define IQ_EST_Q    0x600005e0

static void iq_est_enable(unsigned int mode, unsigned int n) {
    WRITE_REG_MASK(IQ_EST_CTRL, 1);
    unsigned int c = (READ_REG(IQ_EST_CTRL) & 0xfffa0001u) | 2u
                     | (mode << 18) | ((n & 0x7fff) << 2);
    WRITE_REG(IQ_EST_CTRL, c);
    // a dead estimator would wedge boot, so the poll is bounded
    for (unsigned int t = 0; (int)READ_REG(IQ_EST_CTRL) >= 0 && t < 2000000u; t++)
        ;
}

static void iq_est_disable(void) {
    WRITE_REG(IQ_EST_CTRL, (READ_REG(IQ_EST_CTRL) & 0xfffa0001u) | 0x1000u);
    WRITE_REG_UNMASK(IQ_EST_CTRL, 1);
}

static void dc_iq_est(unsigned int num, int *i_out, int *q_out) {
    // a stale done bit would return the previous accumulators and send the servo open-loop, so it is cleared before each sample
    iq_est_disable();
    WRITE_REG_UNMASK(IQ_EST_CTRL, 0x80000000);
    iq_est_enable(1, num);
    *i_out = ((int)READ_REG(IQ_EST_I) >> 6) / (int)(num + 1);
    *q_out = ((int)READ_REG(IQ_EST_Q) >> 6) / (int)(num + 1);
    iq_est_disable();
}

static const unsigned char dco_gain_coarse[7] = { 105, 86, 80, 72, 64, 55, 50 };

static int clamp10(int v) {
    return v < 0 ? 0 : (v > 1023 ? 1023 : v);
}

static void pbus_dco(unsigned int num, unsigned int settle_us, int *out) {
    unsigned int r1 = pbus_rd(3, 1) & 0xff;
    unsigned int gain_lvl = 0;
    for (unsigned int m = r1 & 0x7c; m; m &= m - 1)
        gain_lvl++;
    unsigned int dc_shift1 = (gain_lvl + 6) & 0xff;
    int thr1 = (gain_lvl < 3) ? 2 : (gain_lvl == 3) ? 5 : 10;
    int thr_next = (gain_lvl >= 4) ? 5 : 2;

    unsigned int r2 = (pbus_rd(3, 2) >> 3) & 0xff;
    if (r2 > 6) r2 = 6;
    int gcoarse = dco_gain_coarse[r2];

    pbus_force(4, 2, 0x100);
    pbus_force(5, 2, 0x100);

    int thr = thr1;
    unsigned int dc_shift2 = 4;
    for (int stage = 1; stage <= 2; stage++) {
        int icode = 512, qcode = 512;
        int base = (stage - 1) * 2;
        for (int iter = 12; iter > 0; iter--) {
            int i9 = (icode + 1) >> 1;
            int q9 = (qcode + 1) >> 1;
            out[base + 0] = i9;
            out[base + 1] = q9;
            pbus_force(4, (unsigned int)stage, (unsigned int)i9);
            pbus_force(5, (unsigned int)stage, (unsigned int)q9);
            wait_us(settle_us);
            int iest, qest;
            dc_iq_est(num, &iest, &qest);
            int ai = iest < 0 ? -iest : iest;
            int aq = qest < 0 ? -qest : qest;
            if (ai <= thr && aq <= thr)
                break;
            if (stage == 1) {
                icode -= (gcoarse * iest) >> dc_shift1;
                qcode -= (gcoarse * qest) >> dc_shift1;
            } else {
                icode -= (40 * iest) >> dc_shift2;
                qcode -= (40 * qest) >> dc_shift2;
            }
            icode = clamp10(icode);
            qcode = clamp10(qcode);
        }
        unsigned int nsh = ((r1 >> 2) & 3) + ((r1 >> 4) & 1);
        dc_shift2 = (nsh == 0) ? 4u : (nsh == 1) ? 5u : 6u;
        thr = thr_next;
    }
}

static void rx_gain_table_load(unsigned int do_iq, unsigned int *dc_table,
                                    unsigned int count) {
    unsigned int *gain_lo = dc_table;
    unsigned int *gain_hi = dc_table + 0x40;
    (void)do_iq;
    count &= 0xff;

    // the guard and window writes must precede pbus debug mode
    WRITE_REG_MASK(0x600005c8, 0x00030000);
    WRITE_REG(0x60009a68, 0x000001e0);
    pbus_debug_mode();

    unsigned int saved_12 = rf_i2c_read_mask(0x77, 0, 0x12, 7, 0);
    unsigned int saved_18 = rf_i2c_read_mask(0x77, 0, 0x18, 5, 5) ? 1u : 0u;
    rf_i2c_write_mask(0x77, 0, 0x18, 5, 5, 0);
    rf_i2c_write_mask(0x77, 0, 0x12, 7, 0, 0);

    // pbus_dco can't converge here because the loop is open, so fixed gain_hi and gain_lo tables stand in for measuring
    static const unsigned int gain_hi_tab[60] = {
        0x22700,0x21d00,0x22700,0x21d00,0x22700,0x21b00,0x22700,0x21b00,
        0x22700,0x21b00,0x22700,0x21b00,0x22900,0x21d00,0x22900,0x21d00,
        0x22900,0x21d00,0x22900,0x21d00,0x22900,0x21b02,0x22900,0x21b02,
        0x22900,0x21d00,0x22900,0x21d00,0x22900,0x21d00,0x22900,0x21d00,
        0x22b00,0x21d00,0x22b00,0x21d00,0x22701,0x21b00,0x22701,0x21b00,
        0x22702,0x21b00,0x22702,0x21b00,0x22700,0x21afe,0x22700,0x21afe,
        0x22701,0x21b00,0x22701,0x21b00,0x22702,0x21b00,0x22702,0x21b00,
        0x22700,0x21afe,0x22700,0x21afe,
    };
    (void)pbus_dco; (void)pbus_set_rxgain;
    for (unsigned int k = 0; k < 60; k++)
        gain_hi[k] = gain_hi_tab[k];

    static const unsigned int gain_lo_tab[40] = {
        0x00010000,0x00030002,0x04010400,0x20012000,0x20032002,0x24002004,0x24022401,0x24042403,
        0x28012800,0x28032802,0x2c002804,0x30002c01,0x30023001,0x30043003,0x34013400,0x34033402,
        0x40003404,0x40024001,0x60016000,0x60036002,0x64006004,0x64026401,0x64046403,0x68016800,
        0x68036802,0x70017000,0x70037002,0x70057004,0x70817080,0x70837082,0x70857084,0x70a170a0,
        0x70a370a2,0x70a570a4,0x70a970a8,0x70ab70aa,0x70ad70ac,0x70b970b8,0x70bb70ba,0x70bd70bc,
    };
    for (unsigned int k = 0; k < 40; k++)
        gain_lo[k] = gain_lo_tab[k];
    for (unsigned int k = 40; k < 0x40; k++)
        gain_lo[k] = 0;

    rf_i2c_write_mask(0x77, 0, 0x18, 5, 5, saved_18);
    rf_i2c_write_mask(0x77, 0, 0x12, 7, 0, saved_12);

    WRITE_REG_UNMASK(0x600005c8, 0x00030000);
    pbus_tx_power_off();
    pbus_force(2, 1, 0x184);
    pbus_force(3, 2, 6);
    pbus_work_mode();

    for (unsigned int n = 0; (n & 0xff) < count; ) {
        unsigned int raw;
        unsigned int half = (n & 0xff) >> 1;
        if ((n & 1) == 0)
            raw = ((unsigned int *)gain_lo)[half] & 0xffff;
        else
            raw = (((unsigned int *)gain_lo)[half] >> 16) & 0xffff;

        unsigned int g = (raw & 0x7fff) >> 3;
        unsigned int b = g & 0x7f;
        unsigned int bits = (b >> 6) + ((g & 1)) + ((b >> 5) & 1) + ((b >> 4) & 1) +
                            ((b >> 3) & 1) + ((b >> 2) & 1) + ((b >> 1) & 1);
        if (bits > 4) bits = 4;
        unsigned int idx = bits * 6 + (raw & 7);
        if (idx > 0x1d) idx = 0x1d;

        unsigned int lo = gain_hi[idx * 2];
        unsigned int hi = gain_hi[idx * 2 + 1];

        static const unsigned short bb_step_sp[5] = { 0x07be, 0x07be, 0x07fe, 0x07fe, 0x07fe };
        unsigned int bb_step = bb_step_sp[bits];

        volatile unsigned int *slot =
            (volatile unsigned int *)((n + 0x18002780u) * 4u);

        WRITE_REG(0x60009a68, 0x0000001e);
        *slot = (((lo >> 9) & 0x1ff) * 0x100) + (raw * 0x20000) + ((lo & 0x1ff) >> 1);
        n++;
        WRITE_REG(0x60009a68, 0x000001e0);
        *slot = ((bb_step & 0x7ff) * 4) +
                (((hi >> 9) & 0x1ff) * 0x400000) + (lo * 0x80000000u) +
                ((hi & 0x1ff) * 0x2000);
    }
}

void rx_gain_init(unsigned int rxmax) {
    static int dc_table[0x80];
    static unsigned char rx_max_gain;

    rxmax &= 0xff;

    static const unsigned char gain_idx_tab[16] =
        { 0x00, 0x04, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34,
          0x40, 0x60, 0x64, 0x68, 0x70, 0x74, 0x78, 0x7c };
    static const unsigned char bb_idx_tab[15] =
        { 0x04, 0x02, 0x05, 0x05, 0x05, 0x02, 0x05, 0x05,
          0x03, 0x05, 0x05, 0x04, 0x05, 0x05, 0x04 };

    rx_max_gain = (unsigned char)rx_gain_table_build(dc_table, 0x1c, gain_idx_tab,
                                                   bb_idx_tab,
                                                   (0x10u - rxmax) & 0xff);

    rx_gain_table_load(1, (unsigned int *)dc_table, rx_max_gain + 1u);

    rf_i2c_write(0x77, 0, 0x12, 0xe8);

    WRITE_REG_MASK(0x60009860, 1);

    if (rx_max_gain > 0x55)
        rx_max_gain = 0x55;
    if (rxmax == 0)
        rx_max_gain = 0x46;

    WRITE_REG_RMW(0x60009b48, 0xffffff80, rx_max_gain);
}

void rx_filter_select(int sel) {
    WRITE_REG_UNMASK(0x60009c04, ~0xffffcfefu & 0xffffffffu);
    if (sel == 1)
        WRITE_REG_MASK(0x60009c04, 0x10);
    else if (sel == 2)
        WRITE_REG_MASK(0x60009c04, 0x1000);
    else if (sel == 3)
        WRITE_REG_MASK(0x60009c04, 0x2000);
}
