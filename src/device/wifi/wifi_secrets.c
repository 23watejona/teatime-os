#include "ap_secrets.h"
#include "ipv4.h"

#ifndef NET_MASK
#define NET_MASK "255.255.255.0"
#endif

unsigned char ap_bssid[6];
struct ipv4_addr local_ip;
struct ipv4_addr gw_ip;
struct ipv4_addr net_mask;

static unsigned char hexnib(char c) {
    return (c <= '9') ? c - '0' : (c | 0x20) - 'a' + 10;
}

static void parse_mac(const char *s, unsigned char *out) {
    for (int i = 0; i < 6; i++, s += 3)
        out[i] = (hexnib(s[0]) << 4) | hexnib(s[1]);
}

static void parse_ipv4(const char *s, unsigned char *out) {
    for (int i = 0; i < 4; i++) {
        unsigned int v = 0;
        while (*s >= '0' && *s <= '9')
            v = v * 10 + (*s++ - '0');
        out[i] = v;
        if (*s == '.')
            s++;
    }
}

void wifi_secrets_init(void) {
    parse_mac(AP_BSSID, ap_bssid);
    parse_ipv4(NET_IP, local_ip.bytes);
    parse_ipv4(NET_GW, gw_ip.bytes);
    parse_ipv4(NET_MASK, net_mask.bytes);
}
