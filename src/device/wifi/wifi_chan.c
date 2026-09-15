#include "wait.h"
#include "uart.h"
#include "rf_i2c.h"
#include "reg_util.h"
#include "wifi_regs.h"

// rf pll sdm bytes per channel, so a crystal other than 26 mhz needs a different table
static const unsigned char chan_lo[14][3] = {
    {0x5b, 0xb1, 0x3b}, {0x5b, 0xf2, 0xdf}, {0x5c, 0x34, 0x83}, {0x5c, 0x76, 0x27},
    {0x5c, 0xb7, 0xcb}, {0x5c, 0xf9, 0x6f}, {0x5d, 0x3b, 0x13}, {0x5d, 0x7c, 0xb7},
    {0x5d, 0xbe, 0x5b}, {0x5e, 0x00, 0x00}, {0x5e, 0x41, 0xa4}, {0x5e, 0x83, 0x48},
    {0x5e, 0xc4, 0xec}, {0x5f, 0x62, 0x76},
};

static void pll_reset(void) {
    rf_i2c_write(I2C_RFPLL, 1, 10, 166);
    rf_i2c_write(I2C_RFPLL, 1, 10, 167);
    rf_i2c_write(I2C_RFPLL, 1, 10, 165);
    rf_i2c_write(I2C_RFPLL_SDM, 0,  1, 243);
    rf_i2c_write(I2C_RFPLL, 1, 11, 192);
}

static void pll_write_lo(const unsigned char *lo) {
    rf_i2c_write(I2C_RFPLL_SDM, 0, 0, 7);
    rf_i2c_write(I2C_RFPLL_SDM, 0, 3, lo[0]);
    rf_i2c_write(I2C_RFPLL_SDM, 0, 4, lo[1]);
    rf_i2c_write(I2C_RFPLL_SDM, 0, 5, lo[2]);
    rf_i2c_write(I2C_RFPLL_SDM, 0, 0, 0x17);
}

static void pll_commit(void) {
    rf_i2c_write_mask(I2C_RFPLL, 1, 0, 6, 6, 1);
    rf_i2c_write_mask(I2C_RFPLL, 1, 0, 5, 5, 0);
    rf_i2c_write_mask(I2C_RFPLL, 1, 0, 5, 5, 1);
    rf_i2c_write_mask(I2C_RFPLL, 1, 0, 6, 6, 0);
}

static int pll_wait_locked(void) {
    for (unsigned int i = 0; i < 100; i++) {
        wait_us(20);
        if (rf_i2c_read_mask(I2C_RFPLL, 1, 7, 7, 7))
            return 1;
    }
    return 0;
}

unsigned int g_wifi_channel = 1;

void rx_max_gain_digital(unsigned int ch, int level);

// the pll relock would glitch the cpu clock, so the guard stays set until the lo is reprogrammed
void wifi_set_channel(unsigned int ch) {
    if (ch < 1 || ch > 14)
        return;
    g_wifi_channel = ch;
    WRITE_REG_MASK(RFPLL_CTRL, RFPLL_LATCH);
    // a missed lock leaves the lo off, so retry
    for (int try = 0; try < 8; try++) {
        pll_reset();
        pll_write_lo(chan_lo[ch - 1]);
        pll_commit();
        if (pll_wait_locked())
            break;
        if (try == 7)
            kprintf_uart("wifi: rfpll cal timeout\n");
    }
    WRITE_REG_UNMASK(RFPLL_CTRL, RFPLL_LATCH);

    // v is round(2^19 * 100 / f_mhz), the reciprocal-frequency constant
    static const unsigned short freq_mhz[14] = {
        2412, 2417, 2422, 2427, 2432, 2437, 2442,
        2447, 2452, 2457, 2462, 2467, 2472, 2484
    };
    unsigned int f = freq_mhz[ch - 1];
    unsigned int v = (((100u << 19) + f / 2u) / f) & 0x7fff;
    WRITE_REG_RMW(BB_CHAN_FREQ, 0x00001fff, 0x6000 | (v << 17));

    // the rf handshake isn't up at boot, so the rx compensation only applies on later channel changes
    if (READ_REG(MAC_PHY_CTRL) & MAC_PHY_RF_UP)
        rx_max_gain_digital(ch, 0);
}
