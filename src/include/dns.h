#ifndef DNS_H
#define DNS_H

#include "ipv4.h"

/* Blocks until the server answers; there is no timeout. -1 on a bad name,
   a send failure, an error rcode or no A record. One caller at a time. */
int dns_resolve(const char *name, struct ipv4_addr server, struct ipv4_addr *addr);

#endif
