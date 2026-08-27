#include "ap_secrets.h"

unsigned char ap_bssid[6];
unsigned char our_ip[4];
unsigned char gw_ip[4];

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
    parse_ipv4(NET_IP, our_ip);
    parse_ipv4(NET_GW, gw_ip);
}
