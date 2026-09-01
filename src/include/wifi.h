#ifndef WIFI_H
#define WIFI_H

void init_wifi_clk(void);
void init_wifi_pbus(void);
void init_wifi_iomux(void);
void init_wifi_bb(void);
void init_wifi_mac(void);
void init_wifi_mac_addr(void);
void init_wifi_rf(void);
void wifi_mac_rx_enable(void);
void wifi_secrets_init(void);
void wifi_set_channel(int channel);

void wifi_rx_init(void);
void wifi_rx_servicer(void);
extern int wifi_rx_servicer_pid;

#endif
