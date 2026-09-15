#include "rf_i2c.h"
#include "wifi_regs.h"

// a dead rf register would wedge boot, so the poll is bounded
#define RF_I2C_POLL_MAX 100000u

unsigned int rf_i2c_read(unsigned int block, unsigned int host, unsigned int reg) {
    volatile unsigned int *r = (volatile unsigned int *) RF_I2C_HOST(host);
    *r = block | (reg << 8);
    for (unsigned int t = 0; (*r & RF_I2C_BUSY) && t < RF_I2C_POLL_MAX; t++)
        ;
    return (*r >> 16) & 0xff;
}

void rf_i2c_write(unsigned int block, unsigned int host, unsigned int reg,
                  unsigned int data) {
    volatile unsigned int *r = (volatile unsigned int *) RF_I2C_HOST(host);
    *r = block | (reg << 8) | (data << 16) | RF_I2C_START;
    for (unsigned int t = 0; (*r & RF_I2C_BUSY) && t < RF_I2C_POLL_MAX; t++)
        ;
}

void rf_i2c_write_mask(unsigned int block, unsigned int host, unsigned int reg,
                       unsigned int msb, unsigned int lsb, unsigned int data) {
    unsigned int old = rf_i2c_read(block, host, reg);
    unsigned int mask = (((1u << (msb - lsb + 1)) - 1) << lsb);
    rf_i2c_write(block, host, reg, ((old & ~mask) | ((data << lsb) & mask)) & 0xff);
}

unsigned int rf_i2c_read_mask(unsigned int block, unsigned int host, unsigned int reg,
                              unsigned int msb, unsigned int lsb) {
    unsigned int v = rf_i2c_read(block, host, reg);
    return (v >> lsb) & ((1u << (msb - lsb + 1)) - 1);
}
