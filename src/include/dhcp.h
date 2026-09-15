#ifndef DHCP_H
#define DHCP_H

#include "def.h"
#include "ipv4.h"

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DHCP_BOOTREQUEST 1
#define DHCP_HTYPE_ETHERNET 1
#define DHCP_COOKIE 0x63825363
// some servers drop anything shorter and all pad replies to it, so both directions use this length
#define DHCP_MSG_LEN 300

#define DHCP_OPT_SUBNET_MASK 1
#define DHCP_OPT_ROUTER 3
#define DHCP_OPT_REQUESTED_IP 50
#define DHCP_OPT_MSG_TYPE 53
#define DHCP_OPT_SERVER_ID 54
#define DHCP_OPT_PARAM_REQUEST 55
#define DHCP_OPT_END 255

#define DHCP_DISCOVER 1
#define DHCP_OFFER 2
#define DHCP_REQUEST 3
#define DHCP_ACK 5

struct dhcp_msg {
    u8 op;
    u8 htype;
    u8 hlen;
    u8 hops;
    unsigned int xid;
    unsigned short secs;
    unsigned short flags;
    struct ipv4_addr ciaddr;
    struct ipv4_addr yiaddr;
    struct ipv4_addr siaddr;
    struct ipv4_addr giaddr;
    u8 chaddr[16];
    u8 sname[64];
    u8 file[128];
    unsigned int cookie;
    u8 options[];
} __attribute__((packed));

struct dhcp_opts {
    u8 type_code;
    u8 type_len;
    u8 type;
    u8 param_code;
    u8 param_len;
    u8 params[2];
    u8 req_code;
    u8 req_len;
    u8 req[4];
    u8 sid_code;
    u8 sid_len;
    u8 sid[4];
    u8 end;
} __attribute__((packed));

/* Obtains a lease over broadcast and writes local_ip, gw_ip and net_mask.
   Runs once; no renewal. */
void dhcp_proc(void);

#endif
