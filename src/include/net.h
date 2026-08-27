#ifndef NET_H
#define NET_H

/* Advances the minimal IP bring-up (ARP the gateway, then ICMP-ping it) once the
   4-way handshake has installed keys. Called once per RX-poll iteration. */
void net_tick(void);

/* Delivers a decrypted LLC/SNAP payload (ARP or IP) to the stack. */
void net_input(const unsigned char *llc, unsigned int len);

extern volatile unsigned int net_ping_replies;

#endif
