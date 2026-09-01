#ifndef NET_H
#define NET_H

#include "ipv4.h"

void net_init(void);

void net_tick(void);

void net_recv(unsigned char *llc, unsigned int len);

/* Resolves the next hop over ARP; may park the caller. L3 process context
   only. pkt needs 8 bytes of headroom. */
int net_send(struct ipv4_addr dst, unsigned char *pkt, unsigned int len);

/* To a known link address, no resolution. pkt needs 8 bytes of headroom. */
int net_tx(const unsigned char *mac, unsigned char *pkt, unsigned int len);

int net_tx_llc(const unsigned char *mac, unsigned char *llc, unsigned int len);

#endif
