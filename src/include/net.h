#ifndef NET_H
#define NET_H

#include "ipv4.h"

/* Advances the minimal IP bring-up (ARP the gateway, then ICMP-ping it) once the
   4-way handshake has installed keys. Called once per RX-poll iteration. */
void net_tick(void);

/* Delivers a decrypted LLC/SNAP payload (ARP or IP) to the stack. Servicer
   context. */
void net_recv(unsigned char *llc, unsigned int len);

int net_send_to_gateway(unsigned char *data, unsigned int len);

#endif
