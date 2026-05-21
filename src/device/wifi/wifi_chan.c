#include "wait.h"
#include "uart.h"
#include "rf_i2c.h"
#include "reg_util.h"

// rf pll sdm bytes per channel, so a crystal other than 26 mhz needs a different table
static const unsigned char chan_lo[14][3] = {
    {0x5b, 0xb1, 0x3b}, {0x5b, 0xf2, 0xdf}, {0x5c, 0x34, 0x83}, {0x5c, 0x76, 0x27},
    {0x5c, 0xb7, 0xcb}, {0x5c, 0xf9, 0x6f}, {0x5d, 0x3b, 0x13}, {0x5d, 0x7c, 0xb7},
    {0x5d, 0xbe, 0x5b}, {0x5e, 0x00, 0x00}, {0x5e, 0x41, 0xa4}, {0x5e, 0x83, 0x48},
    {0x5e, 0xc4, 0xec}, {0x5f, 0x62, 0x76},
};

static void pll_reset(void) {
    rf_i2c_write(98, 1, 10, 166);
    rf_i2c_write(98, 1, 10, 167);
    rf_i2c_write(98, 1, 10, 165);
    rf_i2c_write(99, 0,  1, 243);
    rf_i2c_write(98, 1, 11, 192);
}

/* reg-0 frames the SDM load; regs 3/4/5 = channel bytes. */
static void pll_write_lo(const unsigned char *lo) {
    rf_i2c_write(99, 0, 0, 7);
    rf_i2c_write(99, 0, 3, lo[0]);
    rf_i2c_write(99, 0, 4, lo[1]);
    rf_i2c_write(99, 0, 5, lo[2]);
    rf_i2c_write(99, 0, 0, 0x17);
}

/* pulse RF-PLL cal on block 98 reg 0. */
static void pll_commit(void) {
    rf_i2c_write_mask(98, 1, 0, 6, 6, 1);
    rf_i2c_write_mask(98, 1, 0, 5, 5, 0);
    rf_i2c_write_mask(98, 1, 0, 5, 5, 1);
    rf_i2c_write_mask(98, 1, 0, 6, 6, 0);
}

/* poll PLL lock 98/1/7 bit7; warn-only on timeout. */
static void pll_wait_locked(void) {
    for (unsigned int i = 0; i < 100; i++) {
        wait_us(20);
        if (rf_i2c_read_mask(98, 1, 7, 7, 7))
            return;
    }
    kprintf_uart("wifi: rfpll cal timeout\n");
}

/* 0x600005c8 guard brackets the SDM reprogram so the CPU clock doesn't glitch on PLL relock. */
void wifi_set_channel(unsigned int ch) {
    if (ch < 1 || ch > 14)
        return;
    WRITE_REG_MASK(0x600005c8, 0x00f00000);
    pll_reset();
    pll_write_lo(chan_lo[ch - 1]);
    pll_commit();
    pll_wait_locked();
    WRITE_REG_UNMASK(0x600005c8, 0x000f0000);
}
