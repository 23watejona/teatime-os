#include "uart.h"

#include "reg_util.h"

#define CLK_52MHZ (52u)
#define CLK_80MHZ (80u)
#define CLK_160MHZ (160u)

void set_uart0_div(unsigned int clk_rate_mhz, unsigned int baud_rate) {
    unsigned int uart_clk_div = (clk_rate_mhz * 1000000) / baud_rate;
    uart0.clk_div.div_int = uart_clk_div;
    uart0.conf0.x |= 0x6000;
    uart0.conf0.x &= 0xfff9ffff;
}

void set_magic_clk_reg (unsigned int val1, unsigned int val2) {
    WRITE_REG(0x60000d10, 103u | 1u << 8 | val1 << 0x10 | 0x1000000);
    while ((READ_REG(0x60000d00 + 16) & 0x2000000) != 0);
    WRITE_REG(0x60000d10, 103u | 2u << 8 | val2 << 0x10 | 0x1000000);
    while ((READ_REG(0x60000d00 + 16) & 0x2000000) != 0);
}

int init_cpu_clk(unsigned int clk_rate) {
    if (clk_rate != CLK_52MHZ && clk_rate != CLK_80MHZ && clk_rate != CLK_160MHZ) {
        return -1;
    }
    
    set_uart0_div(clk_rate, UART_DEFAULT_BAUD);
    
    // 52mhz is the reset clock, so the pll is left alone
    if (clk_rate == CLK_52MHZ) {
        return 0;
    }

    if (clk_rate == CLK_160MHZ) {
        set_magic_clk_reg(0xc8, 0x91);
    } else {
        set_magic_clk_reg(0x88, 0x91);
    }

    return 0;
}
