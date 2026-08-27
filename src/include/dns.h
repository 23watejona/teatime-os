#ifndef DNS_H
#define DNS_H

#include "ipv4.h"

typedef void (*dns_callback)(const char *name, struct ipv4_addr addr, int ok);

int dns_resolve(const char *name, struct ipv4_addr server, dns_callback cb);

#endif
