#include "reg_util.h"
#include "rf_i2c.h"
#include "wifi_regs.h"
#include "wifi_rxgain.h"

extern void pbus_debug_mode(void);
extern void pbus_work_mode(void);
extern void pbus_force(unsigned int reg, unsigned int width, unsigned int val);
extern void pbus_tx_power_off(void);

// the pbus read window holds three 9-bit registers per word, so a register is found by slot, then by which third of its word
unsigned int pbus_rd(unsigned int reg, unsigned int width) {
    static const unsigned char base_off[8] =
        { 0x00, 0x02, 0x04, 0x05, 0x07, 0x09, 0x0b, 0x0c };
    unsigned int w = (width == 4) ? 2 : width - 1;
    unsigned int slot = w + base_off[reg & 7];
    unsigned int word = READ_REG(PBUS_READ_WINDOW + (slot / 3) * 4);
    unsigned int shift = (slot == 12) ? 0 : 18 - (slot % 3) * 9;
    return (word >> shift) & 0x1ff;
}

static const unsigned short bb_step_tab[8] =
    { 0x0000, 0x0010, 0x0014, 0x0015, 0x0017, 0, 0, 0 };

// packs two 16-bit gain words per table entry and returns the count of words written
static unsigned int rx_gain_table_build(unsigned int *table, unsigned int max_gain,
                                        const unsigned char *gain_idx,
                                        const unsigned char *bb_idx,
                                        unsigned int bb_len) {
    int last = (int)bb_len - 1;

    unsigned int seq = 0;
    unsigned int gain = 0;
    unsigned int pos = 0;

    while (1) {
        if (gain == bb_idx[pos] && (int)pos < last) {
            do {
                pos++;
                if (bb_idx[pos] != 0)
                    break;
            } while ((int)pos < last);
            gain = 0;
        }

        unsigned int frac = gain % 6;
        unsigned int step = gain / 6;
        unsigned int bb = bb_step_tab[step & 7] << 3;
        unsigned int val = (gain_idx[pos] << 8) | bb | frac;

        if (seq & 1)
            table[seq >> 1] |= val << 16;
        else
            table[seq >> 1] = val;

        if (max_gain < gain)
            return seq;

        seq++;
        gain++;
        if (seq == 0x7f)
            return 0x55;
    }
}

static unsigned int popcount(unsigned int v) {
    unsigned int n = 0;
    for (; v; v &= v - 1)
        n++;
    return n;
}

static void rx_gain_table_load(unsigned int *table, unsigned int count) {
    unsigned int *gain_lo = table;
    unsigned int *gain_hi = table + 0x40;

    // the guard and window writes must precede pbus debug mode
    WRITE_REG_MASK(RFPLL_CTRL, 0x00030000);
    WRITE_REG(BB_RX_GAIN_WINDOW, 0x000001e0);
    pbus_debug_mode();

    unsigned int saved_12 = rf_i2c_read_mask(I2C_BB, 0, 0x12, 7, 0);
    unsigned int saved_18 = rf_i2c_read_mask(I2C_BB, 0, 0x18, 5, 5) ? 1u : 0u;
    rf_i2c_write_mask(I2C_BB, 0, 0x18, 5, 5, 0);
    rf_i2c_write_mask(I2C_BB, 0, 0x12, 7, 0, 0);

    // a closed-loop dc servo can't converge with the rx loop open, so fixed gain_hi and gain_lo tables stand in for measuring
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

    rf_i2c_write_mask(I2C_BB, 0, 0x18, 5, 5, saved_18);
    rf_i2c_write_mask(I2C_BB, 0, 0x12, 7, 0, saved_12);

    WRITE_REG_UNMASK(RFPLL_CTRL, 0x00030000);
    pbus_tx_power_off();
    // bit0 brings the tx path up alongside rx but leaves the pa gain off, so the mac keys the pa per burst; continuous drive saturates rx
    pbus_force(2, 1, 0x185);
    pbus_force(3, 2, 6);
    pbus_work_mode();

    for (unsigned int n = 0; n < count; ) {
        unsigned int raw = (n & 1) ? gain_lo[n >> 1] >> 16 : gain_lo[n >> 1] & 0xffff;

        unsigned int g = (raw & 0x7fff) >> 3;
        unsigned int bits = popcount(g & 0x7f);
        if (bits > 4) bits = 4;
        unsigned int idx = bits * 6 + (raw & 7);
        if (idx > 0x1d) idx = 0x1d;

        unsigned int lo = gain_hi[idx * 2];
        unsigned int hi = gain_hi[idx * 2 + 1];

        static const unsigned short bb_step_sp[5] = { 0x07be, 0x07be, 0x07fe, 0x07fe, 0x07fe };
        unsigned int bb_step = bb_step_sp[bits];

        volatile unsigned int *slot =
            (volatile unsigned int *)(BB_RX_GAIN_TABLE + n * 4u);

        WRITE_REG(BB_RX_GAIN_WINDOW, 0x0000001e);
        *slot = (((lo >> 9) & 0x1ff) << 8) | (raw << 17) | ((lo & 0x1ff) >> 1);
        n++;
        WRITE_REG(BB_RX_GAIN_WINDOW, 0x000001e0);
        *slot = ((bb_step & 0x7ff) << 2) | (((hi >> 9) & 0x1ff) << 22) | (lo << 31) |
                ((hi & 0x1ff) << 13);
    }
}

void rx_gain_init(unsigned int rxmax) {
    static unsigned int table[0x80];

    static const unsigned char gain_idx_tab[16] =
        { 0x00, 0x04, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34,
          0x40, 0x60, 0x64, 0x68, 0x70, 0x74, 0x78, 0x7c };
    static const unsigned char bb_idx_tab[15] =
        { 0x04, 0x02, 0x05, 0x05, 0x05, 0x02, 0x05, 0x05,
          0x03, 0x05, 0x05, 0x04, 0x05, 0x05, 0x04 };

    unsigned int rx_max_gain = rx_gain_table_build(table, 0x1c, gain_idx_tab,
                                                   bb_idx_tab, 0x10 - rxmax);

    rx_gain_table_load(table, rx_max_gain + 1);

    rf_i2c_write(I2C_BB, 0, 0x12, 0xe8);

    WRITE_REG_MASK(0x60009860, 1);

    if (rx_max_gain > 0x55)
        rx_max_gain = 0x55;
    if (rxmax == 0)
        rx_max_gain = 0x46;

    WRITE_REG_RMW(BB_RX_MAX_GAIN, 0xffffff80, rx_max_gain);
}

void rx_filter_select(int sel) {
    WRITE_REG_UNMASK(BB_RX_FILTER, 0x3010);
    if (sel == 1)
        WRITE_REG_MASK(BB_RX_FILTER, 0x10);
    else if (sel == 2)
        WRITE_REG_MASK(BB_RX_FILTER, 0x1000);
    else if (sel == 3)
        WRITE_REG_MASK(BB_RX_FILTER, 0x2000);
}
