#ifndef WIFI_TX_H
#define WIFI_TX_H

/* Transmit one raw 802.11 frame (without FCS; hardware appends it).
   Returns 1 on TX-done, 0 on timeout, -1 if too long. */
int wifi_tx_frame(const unsigned char *frame, unsigned int len);

/* Send a wildcard-SSID probe request to the AP. */
int wifi_tx_probe_req(void);

extern volatile unsigned int wifi_tx_done_count;

#endif
