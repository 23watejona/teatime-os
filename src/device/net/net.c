#include "def.h"
#include "uart.h"
#include "string.h"
#include "wifi_ccmp.h"
#include "wifi_wpa.h"
#include "net.h"

/* Minimal IPv4 over the WPA2 link: static address, ARP the gateway for its MAC,
   then ICMP echo it. Everything rides in an LLC/SNAP-framed CCMP data frame to
   the AP. Not a general stack — just enough to prove the link end to end. */

extern unsigned char wifi_mac_addr[6];
extern volatile int wpa_state;
extern unsigned char our_ip[4], gw_ip[4];

static const u8 bcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static u8 gw_mac[6];
static int have_gw;

volatile unsigned int net_ping_replies;
static unsigned int icmp_seq;

enum { NET_IDLE, NET_ARP, NET_PING };
static int state;
static unsigned int last;

static unsigned int ccount(void) {
    unsigned int c;
    __asm__ volatile("rsr.ccount %0" : "=r"(c));
    return c;
}

#define PERIOD_ARP   40000000u /* ~500 ms at 80 MHz */
#define PERIOD_PING  80000000u /* ~1 s */

static unsigned short cksum(const u8 *p, unsigned int n) {
    unsigned int s = 0;
    for (unsigned int i = 0; i + 1 < n; i += 2)
        s += (p[i] << 8) | p[i + 1];
    if (n & 1)
        s += p[n - 1] << 8;
    while (s >> 16)
        s = (s & 0xffff) + (s >> 16);
    return (unsigned short) ~s;
}

/* Prepend the LLC/SNAP header for an ethertype; returns bytes written (8). */
static unsigned int put_snap(u8 *b, unsigned int ethertype) {
    b[0] = 0xaa; b[1] = 0xaa; b[2] = 0x03;
    b[3] = 0x00; b[4] = 0x00; b[5] = 0x00;
    b[6] = ethertype >> 8; b[7] = ethertype & 0xff;
    return 8;
}

static void send_arp_request(void) {
    u8 p[8 + 28];
    unsigned int n = put_snap(p, 0x0806);
    u8 *a = p + n;
    a[0] = 0x00; a[1] = 0x01;
    a[2] = 0x08; a[3] = 0x00;
    a[4] = 6; a[5] = 4;
    a[6] = 0x00; a[7] = 0x01;
    memcpy(a + 8, wifi_mac_addr, 6);
    memcpy(a + 14, our_ip, 4);
    memset(a + 18, 0, 6);
    memcpy(a + 24, gw_ip, 4);
    wifi_ccmp_tx(bcast, p, n + 28);
}

static void send_ping(void) {
    u8 p[8 + 20 + 8];
    unsigned int n = put_snap(p, 0x0800);
    u8 *ip = p + n;
    unsigned int iplen = 20 + 8;
    memset(ip, 0, iplen);
    ip[0] = 0x45; ip[1] = 0x00;
    ip[2] = iplen >> 8; ip[3] = iplen & 0xff;
    ip[4] = 0x00; ip[5] = 0x00;
    ip[6] = 0x00; ip[7] = 0x00;
    ip[8] = 64; ip[9] = 1;
    memcpy(ip + 12, our_ip, 4);
    memcpy(ip + 16, gw_ip, 4);
    unsigned short ic = cksum(ip, 20);
    ip[10] = ic >> 8; ip[11] = ic & 0xff;

    u8 *icmp = ip + 20;
    icmp[0] = 0x08; icmp[1] = 0x00;
    icmp[4] = 0x00; icmp[5] = 0x01;
    icmp[6] = icmp_seq >> 8; icmp[7] = icmp_seq & 0xff;
    icmp_seq++;
    unsigned short cc = cksum(icmp, 8);
    icmp[2] = cc >> 8; icmp[3] = cc & 0xff;

    wifi_ccmp_tx(gw_mac, p, n + iplen);
}

static void send_arp_reply(const u8 *dst_mac, const u8 *dst_ip) {
    u8 p[8 + 28];
    unsigned int n = put_snap(p, 0x0806);
    u8 *a = p + n;
    a[0] = 0x00; a[1] = 0x01;
    a[2] = 0x08; a[3] = 0x00;
    a[4] = 6; a[5] = 4;
    a[6] = 0x00; a[7] = 0x02;
    memcpy(a + 8, wifi_mac_addr, 6);
    memcpy(a + 14, our_ip, 4);
    memcpy(a + 18, dst_mac, 6);
    memcpy(a + 24, dst_ip, 4);
    wifi_ccmp_tx(dst_mac, p, n + 28);
}

void net_input(const unsigned char *llc, unsigned int len) {
    if (len < 8)
        return;
    unsigned int et = (llc[6] << 8) | llc[7];
    const u8 *l3 = llc + 8;
    unsigned int l3len = len - 8;
    kprintf_uart("net: rx et=%x len=%u\n", et, len);

    if (et == 0x0806 && l3len >= 28) {
        unsigned int oper = (l3[6] << 8) | l3[7];
        if (oper == 1 && memcmp(l3 + 24, our_ip, 4) == 0)
            send_arp_reply(l3 + 8, l3 + 14);
        /* Learn the gateway MAC from any ARP it sends — its request for us carries
           the MAC in the sender field too, not only a reply to our request. */
        if (memcmp(l3 + 14, gw_ip, 4) == 0) {
            memcpy(gw_mac, l3 + 8, 6);
            have_gw = 1;
        }
    } else if (et == 0x0800 && l3len >= 28) {
        if (memcmp(l3 + 16, our_ip, 4) == 0)
            kprintf_uart("net: IPv4 TO-US proto=%u src=%u.%u.%u.%u\n",
                         l3[9], l3[12], l3[13], l3[14], l3[15]);
        if (l3[9] == 1) {
            const u8 *icmp = l3 + ((l3[0] & 0x0f) * 4);
            if (icmp[0] == 0x00) {
                net_ping_replies++;
                kprintf_uart("net: PING reply #%u from gateway\n", net_ping_replies);
            }
        }
    }
}

void net_tick(void) {
    if (wpa_state != WPA_DONE)
        return;

    unsigned int now = ccount();
    if (state == NET_IDLE) {
        state = NET_ARP;
        last = now - PERIOD_ARP;
    }

    if (state == NET_ARP) {
        if (have_gw) {
            state = NET_PING;
            last = now - PERIOD_PING;
            kprintf_uart("net: gateway is %x:%x:%x:%x:%x:%x, pinging\n",
                         gw_mac[0], gw_mac[1], gw_mac[2], gw_mac[3], gw_mac[4], gw_mac[5]);
        } else if (now - last >= PERIOD_ARP) {
            last = now;
            kprintf_uart("net: ARP req -> gw\n");
            send_arp_request();
        }
        return;
    }

    if (state == NET_PING && now - last >= PERIOD_PING) {
        last = now;
        send_ping();
    }
}
