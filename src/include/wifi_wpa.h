#ifndef WIFI_WPA_H
#define WIFI_WPA_H

/* Derives the PMK (PBKDF2, 4096 iterations: seconds of CPU). Called once at
   boot, before the watchdog and the servicer exist. */
void wpa_prep(void);

/* Generates the SNonce and arms EAPOL handling; call once on association. */
void wpa_begin(void);

/* Delivers a received frame (RxControl header included) to the 4-way handshake.
   Non-EAPOL frames, and frames received before wpa_begin(), are ignored. */
void wifi_wpa_input(volatile unsigned char *buf, unsigned int len);

/* Delivers a decrypted LLC/SNAP payload carrying EAPOL (ethertype 0x888e) to
   the group-key handshake. Only meaningful once the 4-way is complete. */
void wifi_wpa_eapol(unsigned char *llc, unsigned int len);

/* Drops the handshake state back to WPA_IDLE for a re-join; the PMK is kept. */
void wpa_reset(void);

enum {
    WPA_IDLE,
    WPA_WAIT_M1,
    WPA_WAIT_M3,
    WPA_DONE,
};

extern volatile int wpa_state;

/* Valid once wpa_state == WPA_DONE. */
extern unsigned char wpa_tk[16];
extern unsigned char wpa_gtk[32];
extern unsigned int  wpa_gtk_len;
extern unsigned int  wpa_gtk_id;

#endif
