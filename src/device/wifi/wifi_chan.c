#include "wait.h"

static unsigned int rf_i2c_read(unsigned int block, unsigned int host, unsigned int reg) {
    volatile unsigned int *r = (volatile unsigned int *)(0x60000d00 + host * 4);
    *r = block | (reg << 8);
    while (*r & (1u << 25));
    return (*r >> 16) & 0xff;
}

static void rf_i2c_write(unsigned int block, unsigned int host, unsigned int reg, unsigned int data) {
    volatile unsigned int *r = (volatile unsigned int *)(0x60000d00 + host * 4);
    *r = block | (reg << 8) | (data << 16) | (1u << 24);
    while (*r & (1u << 25));
}

static void rf_i2c_write_mask(unsigned int block, unsigned int host, unsigned int reg,
                              unsigned int msb, unsigned int lsb, unsigned int data) {
    unsigned int old = rf_i2c_read(block, host, reg);
    unsigned int mask = (((1u << (msb - lsb + 1)) - 1) << lsb);
    rf_i2c_write(block, host, reg, ((old & ~mask) | ((data << lsb) & mask)) & 0xff);
}

static const unsigned char chan_lo[14][3] = {
    {0x30, 0x66, 0x66}, {0x30, 0x91, 0x11}, {0x30, 0xbb, 0xbb}, {0x30, 0xe6, 0x66},
    {0x31, 0x11, 0x11}, {0x31, 0x3b, 0xbb}, {0x31, 0x66, 0x66}, {0x31, 0x91, 0x11},
    {0x31, 0xbb, 0xbb}, {0x31, 0xe6, 0x66}, {0x32, 0x11, 0x11}, {0x32, 0x3b, 0xbb},
    {0x32, 0x66, 0x66}, {0x32, 0xcc, 0xcc},
};

static void pll_reset(void) {
    rf_i2c_write(98, 1, 10, 166);
    rf_i2c_write(98, 1, 10, 167);
    rf_i2c_write(98, 1, 10, 165);
    rf_i2c_write(99, 0,  1, 243);
    rf_i2c_write(98, 1, 11, 192);
}

static void pll_write_lo(const unsigned char *lo) {
    rf_i2c_write(99, 0,  7, 0);
    rf_i2c_write(99, 0,  3, lo[0]);
    rf_i2c_write(99, 0,  4, lo[1]);
    rf_i2c_write(99, 0,  5, lo[2]);
    rf_i2c_write(99, 0, 23, 0);
}

static void pll_commit(void) {
    rf_i2c_write_mask(98, 1, 0, 6, 6, 1);
    rf_i2c_write_mask(98, 1, 0, 5, 5, 0);
    rf_i2c_write_mask(98, 1, 0, 5, 5, 1);
    rf_i2c_write_mask(98, 1, 0, 6, 6, 0);
}

void wifi_set_channel(unsigned int ch) {
    if (ch < 1 || ch > 14)
        return;
    pll_reset();
    pll_write_lo(chan_lo[ch - 1]);
    pll_commit();
    wait_us(300);
}
