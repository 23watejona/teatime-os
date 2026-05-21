#ifndef RF_I2C_H
#define RF_I2C_H

unsigned int rf_i2c_read(unsigned int block, unsigned int host, unsigned int reg);
void rf_i2c_write(unsigned int block, unsigned int host, unsigned int reg,
                  unsigned int data);
void rf_i2c_write_mask(unsigned int block, unsigned int host, unsigned int reg,
                       unsigned int msb, unsigned int lsb, unsigned int data);
unsigned int rf_i2c_read_mask(unsigned int block, unsigned int host, unsigned int reg,
                              unsigned int msb, unsigned int lsb);

#endif
