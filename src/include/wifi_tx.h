#ifndef WIFI_TX_H
#define WIFI_TX_H

void wifi_tx_init(void);

/* One 802.11 frame, no FCS. Fills the sequence number and, if Protected,
   the CCMP header. Blocks for TX-done: 0 sent, -1 timeout or too long. */
int wifi_tx_frame(const unsigned char *frame, unsigned int len);

/* Send a wildcard-SSID probe request to the AP. */
int wifi_tx_probe_req(void);

#endif
