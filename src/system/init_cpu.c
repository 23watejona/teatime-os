#include "uart.h"

#include "reg_util.h"
#include "rf_i2c.h"

#define CLK_52MHZ (52u)
#define CLK_80MHZ (80u)
#define CLK_160MHZ (160u)

void set_uart0_div(unsigned int clk_rate_mhz, unsigned int baud_rate) {
    unsigned int uart_clk_div = (clk_rate_mhz * 1000000) / baud_rate;
    uart0.clk_div.div_int = uart_clk_div;
    uart0.conf0.x |= UART_FIFO_RESET;
    uart0.conf0.x &= ~UART_FIFO_RESET;
}

#define I2C_BBPLL 103
#define I2C_BBPLL_HOST 4

static void set_bbpll(unsigned int reg_one, unsigned int reg_two) {
    rf_i2c_write(I2C_BBPLL, I2C_BBPLL_HOST, 1, reg_one);
    rf_i2c_write(I2C_BBPLL, I2C_BBPLL_HOST, 2, reg_two);
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
        set_bbpll(0xc8, 0x91);
    } else {
        set_bbpll(0x88, 0x91);
    }

    return 0;
}
