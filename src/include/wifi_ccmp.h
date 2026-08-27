#ifndef WIFI_CCMP_H
#define WIFI_CCMP_H

/* CCMP (AES-128-CCM) encapsulation for the associated link, done in software:
   the hardware key table is left empty, so protected frames pass through both
   ways and we encrypt/decrypt here with the PTK/GTK from the 4-way handshake. */

/* Install the pairwise TK and group GTK into the hardware key table. Call once
   the 4-way handshake has derived them; required before any protected frame can
   be sent or received. */
void wifi_ccmp_install_keys(void);

/* Encrypt an LLC/SNAP payload and transmit it as a protected ToDS data frame to
   `da` (the final destination MAC; the frame is addressed to the AP at L1).
   Uses the pairwise TK. Returns the wifi_tx_frame result. */
int wifi_ccmp_tx(const unsigned char *da, const unsigned char *payload, unsigned int len);

/* If `buf` (RxControl header included) is a protected data frame from our AP,
   decrypt it and copy the LLC/SNAP payload into out (>= 2048 bytes). Returns the
   payload length, or -1 if the frame is not ours / not protected. */
int wifi_ccmp_rx(volatile unsigned char *buf, unsigned int len, unsigned char *out);

#endif
