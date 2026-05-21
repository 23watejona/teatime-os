#include "reg_util.h"
#include "rf_i2c.h"
#include "wifi_rxgain.h"

extern void pbus_debug_mode(void);
extern void pbus_work_mode(void);
extern void pbus_force(unsigned int a, unsigned int b, unsigned int c);
extern void pbus_tx_power_off(void);
extern void agc_disable(void);

/* pbus readback, 3 nine-bit fields per word. */
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

/* bit-permute a 16-bit gain word into 3 pbus regs. */
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

/* gain-step shift table (5 x u16). */
static const unsigned short bb_step_tab[8] =
    { 0x0000, 0x0010, 0x0014, 0x0015, 0x0017, 0, 0, 0 };

/* software gain-DC table gen; returns stop index (0x55 cap). */
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

/* applies the gain-DC table to BB gain regs.
   TODO(rxgain): per-gain DC measure + RX-IQ servo not ported. */
static void rx_gain_table_load(unsigned int do_iq, unsigned int *dc_table,
                                    unsigned int count) {
    unsigned int *gain_lo = dc_table;
    unsigned int *gain_hi = dc_table + 0x40;
    (void)do_iq;
    count &= 0xff;

    pbus_debug_mode();
    WRITE_REG_MASK(0x600005c8, 0x00030000);
    WRITE_REG(0x60009a68, 0x000001e0);

    unsigned int seen = 0;
    for (unsigned int n = 0; n != count; n = (n + 1) & 0xff) {
        unsigned int raw;
        if ((n & 1) == 0)
            raw = ((unsigned int *)gain_lo)[n >> 1] & 0xffff;
        else
            raw = (((unsigned int *)gain_lo)[n >> 1] >> 16) & 0xffff;

        unsigned int g = (raw & 0x7fff) >> 3;
        unsigned int b = g & 0x7f;
        unsigned int bits = (b >> 6) + ((g & 1)) + ((b >> 5) & 1) + ((b >> 4) & 1) +
                            ((b >> 3) & 1) + ((b >> 2) & 1) + ((b >> 1) & 1);
        if (bits > 4) bits = 4;
        unsigned int idx = bits * 6 + (raw & 7);
        if (idx > 0x1d) idx = 0x1d;

        if ((seen & (1u << (idx & 0x1f))) == 0) {
            if (idx >= 0x18) {
                unsigned int src = idx - 6;
                gain_hi[idx * 2]     = gain_hi[src * 2];
                gain_hi[idx * 2 + 1] = gain_hi[src * 2 + 1];
            } else if ((idx & 1) != 0) {
                unsigned int src = idx - 1;
                gain_hi[idx * 2]     = gain_hi[src * 2];
                gain_hi[idx * 2 + 1] = gain_hi[src * 2 + 1];
            } else {
                /* TODO(rxgain): even slots <0x18 need per-gain DC measure (no HW primitive); slot left zero. */
                pbus_set_rxgain(raw & 0xfff);
                gain_hi[idx * 2]     = 0;
                gain_hi[idx * 2 + 1] = 0;
            }
            seen |= (1u << (idx & 0x1f));
        }
    }

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

        /* no sleep-params BB-step table here -> zero. */
        unsigned int bb_step = 0;

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

/* rxmax = requested RX max gain. */
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

    rf_i2c_write(0x77, 0, 0x12, 0xd8);
    rf_i2c_write_mask(0x77, 0, 0x18, 1, 1, 0);

    WRITE_REG_MASK(0x60009860, 1);

    if (rx_max_gain > 0x55)
        rx_max_gain = 0x55;
    if (rxmax == 0)
        rx_max_gain = 0x46;

    WRITE_REG_RMW(0x60009b48, 0xffffff80, rx_max_gain);
}

/* 11b filter select on 0x60009c04 (1->0x10,2->0x1000,3->0x2000). */
void rx_filter_select(int sel) {
    WRITE_REG_UNMASK(0x60009c04, ~0xffffcfefu & 0xffffffffu);
    if (sel == 1)
        WRITE_REG_MASK(0x60009c04, 0x10);
    else if (sel == 2)
        WRITE_REG_MASK(0x60009c04, 0x1000);
    else if (sel == 3)
        WRITE_REG_MASK(0x60009c04, 0x2000);
}
