#ifndef WIFI_CCMP_H
#define WIFI_CCMP_H

/* CCMP (AES-128-CCM) encapsulation for the associated link. The TK/GTK from the
   4-way handshake go into the hardware key table; the MAC encrypts TX and
   decrypts RX. */

/* Install the pairwise TK and group GTK into the hardware key table. Call once
   the 4-way handshake has derived them; required before any protected frame can
   be sent or received. */
void wifi_ccmp_install_keys(void);

/* Re-install only the group key from wpa_gtk, after a GTK rekey. */
void wifi_ccmp_install_gtk(void);

/* Disable the installed key slots so TX/RX run in the clear again; call on
   link teardown before re-authenticating. */
void wifi_ccmp_clear_keys(void);

/* Encrypt an LLC/SNAP payload and transmit it as a protected ToDS data frame to
   `da` (the final destination MAC; the frame is addressed to the AP at L1).
   Uses the pairwise TK. Returns the wifi_tx_frame result. */
int wifi_ccmp_tx(const unsigned char *da, const unsigned char *payload, unsigned int len);

/* If `buf` (RxControl header included) is a protected data frame from our AP,
   decrypt it and copy the LLC/SNAP payload into out (>= 2048 bytes). Returns the
   payload length, or -1 if the frame is not ours / not protected. */
int wifi_ccmp_rx(volatile unsigned char *buf, unsigned int len, unsigned char *out);

#endif
