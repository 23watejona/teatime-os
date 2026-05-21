#ifndef WIFI_RXGAIN_H
#define WIFI_RXGAIN_H

unsigned int pbus_rd(unsigned int reg, unsigned int width);
void rx_gain_init(unsigned int rxmax);
void rx_filter_select(int sel);

#endif
