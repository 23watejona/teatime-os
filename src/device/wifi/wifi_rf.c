#include "reg_util.h"
#include "wait.h"
#include "rf_i2c.h"
#include "uart.h"
#include "timer.h"
#include "wifi_regs.h"

void wifi_set_channel(unsigned int ch);
void rf_regs_init(void);
void rx_gain_init(unsigned int rxmax);
void init_wifi_pbus(void);
void init_wifi_bb(void);
void tx_rf_enable(void);
unsigned int rx_digital_stop(void);
void rx_digital_start(unsigned int dig_rx_saved);

// the lna and vga gain caps are derived from an analog readback, so they track the individual part
static void rx_max_gain_analog(void) {
    int d = rf_i2c_read(I2C_RFPLL, 1, 5);

    int g0 = (24 * d - 340) / 227;
    if (g0 > 15) g0 = 15;
    if (g0 < 0) g0 = 0;

    int g1 = (23 * d + 29) / 207;
    if (g1 > 15) g1 = 15;

    rf_i2c_write(I2C_RX_GAIN, 0, 4, g0 | 0x40);
    rf_i2c_write(I2C_RX_GAIN, 0, 7, g1 | 0x40);
    rf_i2c_write(97, 1, 5, 0xe0);
}

static void rx_analog_init(void) {
    rf_i2c_write(97, 1, 8, 17);
    rx_max_gain_analog();
}

// takes the rx analog out of cal mode and pushes a starting gain through the latch, so the agc begins from a known point
void rx_path_enable(void) {
    WRITE_REG_RMW(RF_CAL_MODE, 0xfe7fffff, 0);
    wait_us(5);
    WRITE_REG_RMW(BB_RX_GAIN_FORCE, 0xfffffc00, 0xf1);
    WRITE_REG_RMW(BB_RX_GAIN_FORCE, 0xfffffc00, 0xf0);
    wait_us(5);
}

void dpd_bypass(void) {
    rf_i2c_write_mask(I2C_BB, 0, 15, 1, 1, 1);
}

// rf power-down in dependency order: pbus, cal mode, pll, the mac handshake, wifi clocks, then the rf power domain
void rf_off(void) {
    WRITE_REG_RMW(PBUS_CTRL, 0xff0fffff, 0);
    WRITE_REG_RMW(PBUS_CTRL, 0x03ffffff, 0);
    WRITE_REG_RMW(RF_CAL_MODE, 0xfe7fffff, 0x00800000);
    wait_us(1);
    WRITE_REG_UNMASK(RF_CAL_MODE, 0x01800000);
    rf_i2c_write(I2C_RFPLL, 1, 3, 0x01);
    WRITE_REG_UNMASK(MAC_PHY_CTRL, MAC_PHY_RF_UP);
    WRITE_REG_UNMASK(DPORT_CLK_EN, 0x038f0000);
    rtc.rf_pwr = 0x50000000;
}

// brings the rx side of the pbus up while the rf stays in cal mode, so the analog can be programmed before it runs
void rx_pbus_on(void) {
    WRITE_REG_RMW(PBUS_CTRL, 0xff0fffff, 0x00300000);
    WRITE_REG_RMW(PBUS_CTRL, 0x03ffffff, 0xd8000000);
    WRITE_REG_RMW(RF_CAL_MODE, 0xfe7fffff, 0x00800000);
    wait_us(1);
    WRITE_REG_UNMASK(RF_CAL_MODE, 0x01800000);
}

// rf power-up from cold, the reverse of rf_off: clocks and power domains, pll, per-block analog defaults, channel, rx gains, then the mac handshake and a baseband reset
void wifi_rf_on(void) {
    // wifi clock source and enables, and the mac's phy interface, before anything analog is touched
    WRITE_REG_RMW(BBPLL_CTRL, 0xfffffff3, 0x00000004);
    WRITE_REG_MASK(DPORT_CLK_EN, DPORT_WIFI_CLK_EN);
    WRITE_REG_MASK(DPORT_CLK_EN, 0x038f0000);
    WRITE_REG(MAC_CTRL, 0x80000fff);
    WRITE_REG_UNMASK(0x3ff20c74, 0x00c00000);

    WRITE_REG_UNMASK(MAC_PHY_CTRL, MAC_PHY_RF_UP);

    // pll block powered and reset, then every rf power domain on
    rx_pbus_on();
    rf_i2c_write(I2C_RFPLL, 1, 3, 0xf1);
    rf_i2c_write(I2C_RFPLL, 1, 11, 0x80);

    rtc.rf_pwr = 0xfe000000;
    WRITE_REG(0x60000744, 0);
    rtc.pll_ctrl = 0x01000000;
    WRITE_REG_UNMASK(RFPLL_CTRL, 0x00000300);

    rf_i2c_write(101, 4, 0, 0xc6);
    rf_i2c_write_mask(I2C_SAR, 2, 0, 0, 0, 1);

    // analog block defaults need the pll locked on a channel before the rx gains are derived
    WRITE_REG_MASK(DPORT_CLK_EN, 0xffff0000);
    rtc.rf_pwr = 0xfe000000;
    rf_regs_init();
    wifi_set_channel(1);
    rf_i2c_write(97, 1, 7, 0x51);
    rx_analog_init();

    // handshake up and the digital rx reset, so the baseband restarts against the freshly configured analog
    rx_pbus_on();
    WRITE_REG_MASK(MAC_PHY_CTRL, MAC_PHY_RF_UP);

    WRITE_REG_UNMASK(BB_SLEEP, BB_SLEEP_RX);
    WRITE_REG_MASK(BB_RX_CTRL, BB_RX_RESET);
    WRITE_REG_UNMASK(BB_RX_CTRL, BB_RX_RESET);

    rf_i2c_write(I2C_RFPLL, 1, 6, 0x08);
    rf_i2c_write(I2C_RFPLL, 1, 9, 0x10);

    if (READ_REG(MAC_PHY_CTRL) & MAC_PHY_RF_UP)
        rx_path_enable();

    // the rf off/on cycle above resets the tx enable, so it is redone here with the rx datapath stopped, since poking the shared tx analog with rx up wedges both
    unsigned int dig = rx_digital_stop();
    tx_rf_enable();
    rx_digital_start(dig);
}

// measures the on-chip rc time constant and writes matching filter trims, so the baseband filter corners land on their design bandwidth despite process spread
void rc_calibrate(void) {
    kprintf_uart("      rc:w1\n");
    rf_i2c_write_mask(106, 2, 0, 5, 4, 0);
    rf_i2c_write_mask(106, 2, 4, 7, 4, 1);
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

    int rc_a = (16 * (int)v - 39) / 30;
    int rc_b = (28 * v / 9 + 2) >> 2;

    rf_i2c_write(97,  1, 2, (rc_a | 0xa0) & 0xff);
    rf_i2c_write(102, 3, 1, (rc_b | 0x40) & 0xff);
}

// the sar adc measures tx dc offset and power, so it comes up with the rf
static void sar_init(void) {
    rtc.rf_pwr |= 0x02000000;
    rf_i2c_write_mask(I2C_SAR, 2, 0, 4, 4, 1);
    rf_i2c_write_mask(I2C_SAR, 2, 1, 1, 0, 2);
}

// tx chain analog block defaults
static void tx_regs_init(void) {
    rf_i2c_write(I2C_TX, 2, 1, 104);
    rf_i2c_write(I2C_TX, 2, 2, 15);
    rf_i2c_write(I2C_TX, 2, 3, 168);
    rf_i2c_write(I2C_TX, 2, 4, 6);
    rf_i2c_write(I2C_TX, 2, 5, 8);
    rf_i2c_write(I2C_TX, 2, 6, 184);
    rf_i2c_write(I2C_TX, 2, 7, 91);
    rf_i2c_write(I2C_TX, 2, 8, 4);
    rf_i2c_write(I2C_TX, 2, 9, 0);
    rf_i2c_write(I2C_TX, 2, 10, 116);
    rf_i2c_write(I2C_TX, 2, 11, 7);
}

// bias, tx chain, rx gain, sar and pll defaults for every analog block, in the order the blocks depend on each other
void rf_regs_init(void) {
    rf_i2c_write(106, 2, 0, 37);
    tx_regs_init();
    rf_i2c_write(I2C_TX, 2, 1, 72);
    rf_i2c_write_mask(I2C_RX_GAIN, 0, 5, 1, 0, 3);
    rf_i2c_write(97, 1, 8, 17);
    rf_i2c_write(I2C_SAR, 2, 0, 21);
    sar_init();
    rf_i2c_write_mask(103, 4, 4, 7, 7, 1);
    rf_i2c_write(106, 2, 0, 41);
    rf_i2c_write(I2C_RFPLL, 1, 4, 175);
    rf_i2c_write(I2C_RFPLL, 1, 11, 128);
    WRITE_REG_RMW(BBPLL_CTRL, 0xfffff9ff, 0x400);
}

// a force-write drives one pbus register directly, so the baseband's own control of it is overridden until pbus_work_mode
void pbus_force(unsigned int reg, unsigned int width, unsigned int val) {
    unsigned int v = (READ_REG(PBUS_CTRL) & 0xffff0001) | PBUS_FORCE_STROBE;
    v |= (reg & 0xff) << PBUS_FORCE_REG_SHIFT;
    v |= (val & 0xffff) << PBUS_FORCE_VAL_SHIFT;
    v |= (width & 0xff) << PBUS_FORCE_WIDTH_SHIFT;
    WRITE_REG(PBUS_CTRL, v);
    for (unsigned int t = 0; (READ_REG(PBUS_STATUS) & PBUS_BUSY) && t < 100000u; t++)
        ;
    // only the force-test strobe is cleared; debug mode stays on until pbus_work_mode
    WRITE_REG_UNMASK(PBUS_CTRL, PBUS_FORCE_STROBE);
}

// tx gains to their floor, so the rx side can be calibrated without the transmitter leaking into it
void pbus_tx_power_off(void) {
    pbus_force(6, 1, 0);
    pbus_force(1, 1, 12);
    pbus_force(2, 1, 128);
}

// zero baseband attenuation in every rate slot, so the tx level is set by the rf gain alone
#define TX_BB_ATTEN 0
static void tx_bb_atten_max(void) {
    for (unsigned int i = 0; i < RF_TXPWR_REGS; i++) {
        unsigned int a = RF_TXPWR_REG(i);
        WRITE_REG(a, (READ_REG(a) & 0xffffff00u) | TX_BB_ATTEN);
    }
}

void tx_rf_enable(void) {
    rf_i2c_write_mask(I2C_BB, 0, 28, 6, 6, 1); // tx bb clock
    rf_i2c_write_mask(124, 1, 21, 0, 0, 1);
    rf_i2c_write_mask(I2C_BB, 0, 9, 7, 0, 0); // overflow trim, not the drive level
    tx_bb_atten_max();
}

void pbus_work_mode(void) {
    WRITE_REG_UNMASK(PBUS_CTRL, PBUS_DEBUG_MODE);
    WRITE_REG_UNMASK(BB_SLEEP, BB_SLEEP_RX);
}

extern unsigned int pbus_rd(unsigned int reg, unsigned int width);

// blocks until the pbus reports ready, so a following force-test can't land on a not-yet-ready bus
void pbus_debug_mode(void) {
    unsigned int v = READ_REG(PBUS_CTRL);
    if ((v & PBUS_DEBUG_MODE) == 0 && (READ_REG(MAC_PHY_CTRL) & MAC_PHY_RF_UP)) {
        for (unsigned int g = 0; g < 100000u; g++) {
            wait_us(5);
            if ((pbus_rd(2, 1) & 0x184) == 0x184 && (pbus_rd(3, 2) & 6) == 6)
                break;
        }
    }
    WRITE_REG_MASK(BB_SLEEP, BB_SLEEP_RX);
    WRITE_REG_MASK(PBUS_CTRL, PBUS_DEBUG_MODE);
    if (READ_REG(MAC_PHY_CTRL) & MAC_PHY_RF_UP) {
        for (unsigned int t = 0; t < 100000u; t++)
            if (READ_REG(PBUS_STATUS) & PBUS_READY)
                break;
    }
}

// per-channel rx gain trim, one step more on the upper channels and six more at the extended gain level
void rx_chan_compensate(unsigned int ch, int level) {
    int base = (ch >= 11 && ch <= 13) ? -6 : -7;
    int ext = (level == 1) ? -6 : 0;
    unsigned int cmp = (base + ext) & 0xff;
    WRITE_REG_RMW(BB_RX_CHAN_COMP, 0xfffc03ff, cmp << 10);
    WRITE_REG_MASK(BB_RX_GAIN_FORCE, BB_RX_GAIN_LATCH);
    WRITE_REG_UNMASK(BB_RX_GAIN_FORCE, BB_RX_GAIN_LATCH);
}

void rx_max_gain_digital(unsigned int ch, int level) {
    if (level)
        WRITE_REG_UNMASK(RF_RX_GAIN_EXT, RF_RX_GAIN_EXT_DIG);
    else
        WRITE_REG_MASK(RF_RX_GAIN_EXT, RF_RX_GAIN_EXT_DIG);
    rx_chan_compensate(ch, level);
}

// first-ever rf init: the rf power domains come up one group at a time with the mac handshake down, then the analog is programmed through the pbus and the pll put on channel 1
void rf_init(void) {
    WRITE_REG_MASK(DPORT_CLK_EN, 0xffff0000);

    // rtc pll timing and the first rf power domains, with the rf held in cal mode
    rtc.pll_ctrl = 0x0019c06a;
    rtc.rf_pwr = 0xf0000000;
    WRITE_REG_UNMASK(RF_CAL_MODE, 0x01800000);
    WRITE_REG_UNMASK(RF_CAL_MODE, 0x08000000);
    rtc.rf_pwr |= 0x02000000;

    // pbus timing before the last domains, so the bus is stable when the analog blocks wake
    rtc.rf_pwr |= 0x30000000;
    WRITE_REG_RMW(PBUS_STATUS, 0xe0ffffff, 0x1c000000);
    WRITE_REG_RMW(PBUS_CFG, 0xcfffffff, 0x10000000);
    WRITE_REG_UNMASK(MAC_PHY_CTRL, MAC_PHY_RF_UP);
    wait_us(2);
    rtc.rf_pwr |= 0x0c000000;

    // tx path enabled but at zero gain for the rest of bring-up
    pbus_debug_mode();
    pbus_force(2, 1, 129);
    pbus_tx_power_off();
    pbus_work_mode();

    rf_i2c_write(I2C_RFPLL, 1, 0, 40);
    wifi_set_channel(1);
    rx_analog_init();
    rf_regs_init();
    rf_i2c_write(I2C_BB, 0, 26, 8);
    rf_i2c_write(I2C_BB, 0, 26, 56);
}

// the offsets are shifted as signed values, so a negative one also sets every bit above its field
static void tx_dc_offset_apply(unsigned int gain, const signed char *offset) {
    int i_off = offset[0];
    int q_off = offset[1];
    for (unsigned int i = 0; i < RF_TXPWR_REGS; i++) {
        volatile unsigned int *pwr = (volatile unsigned int *) RF_TXPWR_REG(i);
        *pwr = (*pwr & 0xfff000ff) | (gain << 8);
        volatile unsigned int *dc = (volatile unsigned int *) RF_TX_DC_REG(i);
        if (i & 1)
            *dc = (*dc & 0xf0003fff) | (q_off << 21) | (i_off << 14);
        else
            *dc = (*dc & 0xffffc000) | (q_off << 7) | i_off;
    }
}

#define MCAL_REG       0x60000d4c
#define MCAL_ARM       0x01113cf1
#define MCAL_TRIGGER   0x01113cf3
#define MCAL_STOP      0x01113cf0
#define MCAL_DONE      0x01000000
#define MCAL_QFLAG     0x40000000
#define DC_CAL_SAMPLES 12u
#define DC_CAL_AVG     4u
#define DC_POLL_CAP    100000u

static int dc_clamp_s8(int v) {
    if (v > 127) return 127;
    if (v < 0) return 0;
    return v;
}

// binary search on the i and q dc codes: the mac's cal engine only reports the sign of the residual, so each step halves and the last four codes are averaged
static void tx_dc_offset_measure(unsigned int gain, signed char out[2]) {
    int acc_i = 0, acc_q = 0;
    int code_i = 64, code_q = 64;
    int step = 28;

    pbus_force(0, 1, gain);

    for (unsigned int n = 0; n < DC_CAL_SAMPLES; n++) {
        pbus_force(1, 2, code_q);
        pbus_force(0, 2, code_i);

        WRITE_REG(MCAL_REG, MCAL_ARM);
        WRITE_REG(MCAL_REG, MCAL_TRIGGER);
        wait_us(2);

        for (unsigned int t = 0; t < DC_POLL_CAP; t++)
            if (READ_REG(MCAL_REG) & MCAL_DONE)
                break;

        unsigned int sign_i = READ_REG(MCAL_REG);
        unsigned int sign_q = READ_REG(MCAL_REG); // read twice on purpose: the first read's sign steers i, the second read's q flag steers q

        if ((int)sign_i < 0) code_i -= step;
        else                 code_i += step;
        code_i = dc_clamp_s8(code_i);

        if (sign_q & MCAL_QFLAG) code_q -= step;
        else                     code_q += step;
        code_q = dc_clamp_s8(code_q);

        step = (step == 2) ? 1 : (step >> 1) + 1;

        if (n >= DC_CAL_SAMPLES - DC_CAL_AVG) {
            acc_i += code_i;
            acc_q += code_q;
        }
    }

    acc_q = (acc_q + 2) >> 2;
    acc_i = (acc_i + 2) >> 2;

    pbus_force(1, 2, acc_q);
    pbus_force(0, 2, acc_i);
    pbus_force(1, 1, 127);
    WRITE_REG(MCAL_REG, MCAL_STOP);

    out[0] = acc_i;
    out[1] = acc_q;
}

static unsigned int tx_gain_to_dc_index(unsigned int gain) {
    static const unsigned char dc_index_tab[17] = {
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };
    unsigned int i = gain - 4;
    return (i > 16) ? 1 : dc_index_tab[i];
}

// the tx gain hasn't been written yet at this point, so the dc offset is calibrated against its power-on default
#define TX_RF_ANA_GAIN 0x0bf0u

// nulls the tx carrier leak: the offset is measured at four rf gains and the pair for the gain in use goes into every rate slot
static void tx_dc_offset_calibrate(void) {
    static const unsigned char gain_tbl[4] = {0x04, 0x10, 0x12, 0x14};
    signed char pairs[4][2];

    pbus_debug_mode();
    pbus_force(1, 1, 31);

    for (unsigned int g = 0; g < 4; g++)
        tx_dc_offset_measure(gain_tbl[g], pairs[g]);

    unsigned int idx = tx_gain_to_dc_index(TX_RF_ANA_GAIN & 0x1f);
    if (idx > 3) idx = 3;
    signed char sel[2];
    sel[0] = pairs[idx][0];
    sel[1] = pairs[idx][1];
    pbus_force(0, 2, sel[0]);
    pbus_force(1, 2, sel[1]);

    tx_dc_offset_apply(TX_RF_ANA_GAIN, sel);
    kprintf_uart("dcoff: p0=%d,%d p1=%d,%d p2=%d,%d p3=%d,%d sel[%d]=%d,%d\n",
                 pairs[0][0], pairs[0][1], pairs[1][0], pairs[1][1],
                 pairs[2][0], pairs[2][1], pairs[3][0], pairs[3][1],
                 idx, sel[0], sel[1]);

    pbus_work_mode();
}

static void noise_floor_set(int nf) {
    WRITE_REG_UNMASK(BB_RX_CTRL, BB_NOISE_MEAS);
    int v = (nf + 1) / 2;
    WRITE_REG_RMW(BB_NOISE_FLOOR, 0xfffffe00, v & 0x1ff);
    WRITE_REG_RMW(BB_RX_CTRL, 0xfffd7ffd, BB_NOISE_MEAS);
}

static int noise_floor_get(void) {
    int v = (READ_REG(BB_NOISE_FLOOR) >> 20) & 0xfff;
    return ((v + 1) >> 1) - 0x800;
}

static int noise_floor_clamped(void) {
    int v = noise_floor_get();
    if (v >= -340) return -340;
    if (v <  -392) return -392;
    return v;
}

static void noise_floor_start(unsigned int ch3) {
    if (READ_REG(BB_RX_CTRL) & BB_NOISE_MEAS)
        return;
    WRITE_REG_RMW(BB_NOISE_FLOOR, 0xfffff000, ((ch3 & 7) << 9) | 0x1a0);
    WRITE_REG_MASK(BB_RX_CTRL, 0x00028002);
}

static int noise_floor_measure(unsigned int chan) {
    unsigned int n;
    if (READ_REG(BB_RX_CTRL) & BB_NOISE_MEAS) {
        for (n = 0; n < 20000; n++) {
            if (!(READ_REG(BB_RX_CTRL) & BB_NOISE_MEAS))
                break;
            wait_us(1);
        }
        if (n >= 20000)
            return 1;
    }
    wait_us(2);
    noise_floor_start(chan);
    for (n = 0; n < 20000; n++) {
        if (!(READ_REG(BB_RX_CTRL) & BB_NOISE_MEAS))
            return 0;
        wait_us(1);
    }
    return 1;
}

// measures the noise floor four times on channel 1 with the rx loop forced open and keeps the lowest, so the cca threshold sits just above the real floor of this board
static void noise_init(void) {
    noise_floor_set(-388);
    WRITE_REG_RMW(BB_RX_GAIN_FORCE, 0xfffffc00, 201);
    WRITE_REG_UNMASK(BB_RX_GAIN_FORCE, BB_RX_GAIN_LATCH);

    int nf[4];
    nf[0] = nf[1] = nf[2] = nf[3] = -340;
    unsigned int saved_dig_rx = READ_REG(BB_DIG_RX);
    unsigned int saved_d20 = READ_REG(0x60009d20);
    unsigned int saved_d40 = READ_REG(0x60009d40);

    WRITE_REG_UNMASK(BB_DIG_RX, 1);
    WRITE_REG_UNMASK(0x60009d20, 0x40000000);
    wifi_set_channel(1);

    int ok = 0;
    for (int g = 0; g < 4; g++) {
        if (!noise_floor_measure(1)) {
            ok = 1;
            int s = noise_floor_clamped();
            if (s < nf[g])
                nf[g] = s;
        }
        WRITE_REG_UNMASK(BB_RX_CTRL, BB_NOISE_MEAS);
    }

    WRITE_REG(BB_DIG_RX, saved_dig_rx);
    WRITE_REG(0x60009d20, saved_d20);
    WRITE_REG(0x60009d40, saved_d40);

    if (ok) {
        int mn = nf[0];
        for (int i = 1; i < 4; i++)
            if (nf[i] < mn) mn = nf[i];
        if (mn > -40) mn = -40;
        noise_floor_set(mn);
    }
}

int live_rssi(void) {
    WRITE_REG_UNMASK(BB_RX_CTRL, BB_NOISE_MEAS);
    noise_floor_measure(1);
    int v = (int)(READ_REG(BB_RX_RSSI) & 0xfff) - 0xfff;
    return v >> 1;
}

// baseband bring-up in the order the steps depend on each other: pbus, rc cal, tx dc cal, rx baseband config, noise floor, then predistortion bypassed
void bb_bringup(void) {
    rf_i2c_write(97, 1, 7, 81);
    init_wifi_pbus();
    rf_i2c_write_mask(I2C_BB, 0, 16, 0, 0, 1);
    kprintf_uart("    bb: rc_calibrate\n");
    rc_calibrate();
    kprintf_uart("    bb: dcoffset\n");
    tx_dc_offset_calibrate();
    kprintf_uart("    bb: init_wifi_bb\n");
    init_wifi_bb();
    WRITE_REG_MASK(MAC_PHY_CTRL, MAC_PHY_RF_UP);
    kprintf_uart("    bb: noise_init\n");
    noise_init();
    kprintf_uart("    bb: dpd_bypass\n");
    dpd_bypass();
}

void antenna_switch_init(void) {
    // left at reset the antenna switch never keys to tx, so the patterns are set here
    WRITE_REG(BB_ANT_SWITCH_LO, 0x01010101);
    WRITE_REG(BB_ANT_SWITCH_HI, 0x04010101);
    WRITE_REG_MASK(BB_AGC_CTRL, 0x00800000);
    WRITE_REG_RMW(BB_SLEEP, 0xffffc3ff, 0x00000800);
    WRITE_REG(BB_TX_CAL, 2);
}

// relocks the baseband pll with the cpu double clock held off, so the cpu clock doesn't glitch while the pll is out
void bbpll_calibrate(unsigned int slow) {
    unsigned int saved = READ_REG(DPORT_CTL_REG);
    WRITE_REG(DPORT_CTL_REG, saved & ~DPORT_CTL_DOUBLE_CLK);
    wait_us(1);
    WRITE_REG_RMW(BBPLL_CTRL, 0xfffffff3, 8);
    wait_us(slow ? 1000 : 100);
    WRITE_REG_RMW(BBPLL_CTRL, 0xfffffff3, 4);
    wait_us(1);
    WRITE_REG(DPORT_CTL_REG, saved);
}

// freezes the digital rx while the analog is changed, so a half-configured path can't latch a bad gain
unsigned int rx_digital_stop(void) {
    unsigned int saved = READ_REG(BB_DIG_RX);
    WRITE_REG_MASK(BB_SLEEP, BB_SLEEP_RX);
    WRITE_REG_UNMASK(BB_DIG_RX, BB_DIG_RX_EN);
    return saved;
}

void rx_digital_start(unsigned int dig_rx_saved) {
    WRITE_REG_UNMASK(BB_SLEEP, BB_SLEEP_RX);
    WRITE_REG_MASK(BB_RX_CTRL, BB_RX_RESET);
    WRITE_REG_UNMASK(BB_RX_CTRL, BB_RX_RESET);
    WRITE_REG(BB_DIG_RX, dig_rx_saved);
}

void agc_enable(void) {
    WRITE_REG_UNMASK(BB_AGC_CTRL, BB_AGC_OFF);
}

void agc_disable(void) {
    WRITE_REG_MASK(BB_AGC_CTRL, BB_AGC_OFF);
}

void rx_clock_enable(unsigned int en) {
    rf_i2c_write_mask(I2C_BB, 0, 28, 5, 5, en);
    rf_i2c_write_mask(124, 1, 21, 1, 1, en);
}

static void powerup_option_set(void) {
    rtc.scratch[3] = 3;
}

// there is no second-stage bootloader to set this analog default, so it is set here or rx evm degrades
static void analog_reg_default(void) {
    WRITE_REG_MASK(0x60000d48, 1);
    WRITE_REG_UNMASK(0x60000d48, 1);
    unsigned int e = READ_REG(EFUSE_DATA2_REG);
    WRITE_REG(BB_ANALOG_DEFAULT,(((e >> 12) & 0xa) == 0xa) ? 0xe690a568 : 0xeab4d027);
}

void init_wifi_rf(void) {
    kprintf_uart("  rf: analog_reg_default\n");
    analog_reg_default();
    kprintf_uart("  rf: powerup_option_set\n");
    powerup_option_set();
    // must run before rf init
    kprintf_uart("  rf: antenna_switch_init\n");
    antenna_switch_init();
    kprintf_uart("  rf: rf_init\n");
    rf_init();
    kprintf_uart("  rf: bb_bringup\n");
    bb_bringup();
    rf_i2c_write_mask(103, 4, 4, 4, 0, 0x13);
    kprintf_uart("  rf: rx_digital_stop\n");
    unsigned int dig = rx_digital_stop();
    kprintf_uart("  rf: bbpll_calibrate\n");
    bbpll_calibrate(0);
    // the rx path wants the bb rx clock off, so it is explicitly left at its reset default
    kprintf_uart("  rf: rx_clock_enable\n");
    rx_clock_enable(0);
    kprintf_uart("  rf: rx_path_enable\n");
    rx_path_enable();
    kprintf_uart("  rf: rx_digital_start\n");
    rx_digital_start(dig);
    kprintf_uart("  rf: agc_enable\n");
    agc_enable();

    WRITE_REG_UNMASK(BB_RX_CTRL, BB_NOISE_MEAS);
    noise_floor_start(1);
}
