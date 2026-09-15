#include "timer.h"
#include "reg_util.h"
#include "wifi_regs.h"

extern int init_cpu_clk(unsigned int clk_rate_mhz);

void init_wifi_clk(void) {
    WRITE_REG_MASK(DPORT_CLK_EN, DPORT_WIFI_CLK_EN);

    rtc.sleep_mask = 0xffffffff;

    WRITE_REG(0x60000718, (READ_REG(0x60000718) & ~0x3Fu) | 8);
    WRITE_REG_UNMASK(0x600007a8, 0x1);

    rtc.pwr = 0x00046046;
    rtc.slp_val = rtc.slp_cnt_val + 1000;

    rtc.analog_0 |= DPORT_WIFI_CLK_EN;
    for (unsigned int t = 0; (rtc.status & 0x3) == 0 && t < 100000u; t++)
        ;

    rtc.timing[0] = 0x20302020;
    rtc.timing[1] = 0x20500000;

    rtc.analog_1 = 0;
    rtc.analog_2 = 7;
    rtc.analog_3 = 7;

    rtc.gpio_out = 0;
    rtc.gpio_enable = 0;
    rtc.analog_6 = 0;
    rtc.gpio_conf = 0;
    rtc.clk_1 = 0;
    rtc.clk_2 = 0;
    rtc.clk_3 = 0;
    rtc.trim[0] = 0;
    rtc.trim[1] = 0;
    rtc.trim[2] = 0;

    rtc.analog_0 = 0;
    rtc.sleep_state = 0;

    init_cpu_clk(CPU_MHZ);
}
