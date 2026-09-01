#include "def.h"
#include "uart.h"
#include "string.h"
#include "wifi_ccmp.h"
#include "wifi_wpa.h"
#include "net.h"
#include "ipv4.h"
#include "udp.h"
#include "icmp.h"

#define MTU (1518)

#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPV4 0x0800

#define LLC_SNAP_LEN 8

#define ARP_HTYPE_ETHERNET 1
#define ARP_OPER_REQUEST 1
#define ARP_OPER_REPLY   2

extern unsigned char wifi_mac_addr[6];
extern volatile int wpa_state;
extern struct ipv4_addr local_ip;
extern struct ipv4_addr gw_ip;

static const u8 bcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static u8 gw_mac[6];
static int have_gw;

struct arp_pkt {
    unsigned short htype;
    unsigned short ptype;
    u8 hlen;
    u8 plen;
    unsigned short oper;
    u8 sender_mac[6];
    struct ipv4_addr sender_ip;
    u8 target_mac[6];
    struct ipv4_addr target_ip;
} __attribute__((packed));

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

static void put_snap(u8 *b, unsigned int ethertype) {
    b[0] = 0xaa; b[1] = 0xaa; b[2] = 0x03;
    b[3] = 0x00; b[4] = 0x00; b[5] = 0x00;
    b[6] = ethertype >> 8; b[7] = ethertype & 0xff;
}

static void send_arp_request(void) {
    u8 p[LLC_SNAP_LEN + sizeof(struct arp_pkt)];
    put_snap(p, ETHERTYPE_ARP);
    struct arp_pkt *a = (struct arp_pkt *) (p + LLC_SNAP_LEN);
    a->htype = htons(ARP_HTYPE_ETHERNET);
    a->ptype = htons(ETHERTYPE_IPV4);
    a->hlen = sizeof(a->sender_mac);
    a->plen = sizeof(a->sender_ip);
    a->oper = htons(ARP_OPER_REQUEST);
    memcpy(a->sender_mac, wifi_mac_addr, 6);
    a->sender_ip = local_ip;
    memset(a->target_mac, 0, 6);
    a->target_ip = gw_ip;
    wifi_ccmp_tx(bcast, p, sizeof(p));
}

static void send_ping(void) {
    u8 p[LLC_SNAP_LEN + sizeof(union ipv4_header) + sizeof(struct icmp_echo)];
    put_snap(p, ETHERTYPE_IPV4);
    union ipv4_header *h = (union ipv4_header *) (p + LLC_SNAP_LEN);
    memset(h, 0, sizeof(*h));
    h->fields.version_ihl = 0x45;
    h->fields.total_len = htons(sizeof(union ipv4_header) + sizeof(struct icmp_echo));
    h->fields.ttl = 64;
    h->fields.protocol = IPPROTO_ICMP;
    h->fields.src_addr = local_ip.word;
    h->fields.dest_addr = gw_ip.word;
    h->fields.checksum = checksum(h->raw, sizeof(*h), 0);

    struct icmp_echo *e = (struct icmp_echo *) (h->raw + sizeof(*h));
    e->type = ICMP_ECHO_REQUEST;
    e->code = 0;
    e->checksum = 0;
    e->ident = htons(1);
    e->seq = htons(icmp_seq);
    icmp_seq++;
    e->checksum = checksum(e, sizeof(*e), 0);

    wifi_ccmp_tx(gw_mac, p, sizeof(p));
}

static void arp_recv(u8 *payload, unsigned int len) {
    if (len < sizeof(struct arp_pkt))
        return;
    struct arp_pkt *a = (struct arp_pkt *) payload;
    /* Learn the gateway MAC from any ARP it sends — its request for us carries
       the MAC in the sender field too, not only a reply to our request. */
    if (a->sender_ip.word == gw_ip.word) {
        memcpy(gw_mac, a->sender_mac, 6);
        have_gw = 1;
    }
    if (ntohs(a->oper) == ARP_OPER_REQUEST && a->target_ip.word == local_ip.word) {
        u8 req_mac[6];
        memcpy(req_mac, a->sender_mac, 6);
        struct ipv4_addr req_ip = a->sender_ip;
        a->oper = htons(ARP_OPER_REPLY);
        memcpy(a->target_mac, req_mac, 6);
        a->target_ip = req_ip;
        memcpy(a->sender_mac, wifi_mac_addr, 6);
        a->sender_ip = local_ip;
        put_snap(payload - LLC_SNAP_LEN, ETHERTYPE_ARP);
        wifi_ccmp_tx(req_mac, payload - LLC_SNAP_LEN,
                     LLC_SNAP_LEN + sizeof(struct arp_pkt));
    }
}

void net_recv(unsigned char *llc, unsigned int len) {
    if (len < LLC_SNAP_LEN)
        return;
    unsigned int et = ntohs(*(const unsigned short *) (llc + LLC_SNAP_LEN - 2));
    switch (et) {
    case ETHERTYPE_ARP:
        arp_recv(llc + LLC_SNAP_LEN, len - LLC_SNAP_LEN);
        break;
    case ETHERTYPE_IPV4: {
        u8 *ip = llc + LLC_SNAP_LEN;
        len -= LLC_SNAP_LEN;
        if (len < sizeof(union ipv4_header))
            return;
        union ipv4_header *h = (union ipv4_header *) ip;
        unsigned int total_len = ntohs(h->fields.total_len);
        if (total_len > len)
            return;
        ipv4_enqueue(ip, total_len);
        break;
    }
    default:
        break;
    }
}

void net_tick(void) {
    if (wpa_state != WPA_DONE)
        return;

    unsigned int now = ccount();
    switch (state) {
    case NET_IDLE:
        state = NET_ARP;
        last = now - PERIOD_ARP;
    case NET_ARP:
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
    case NET_PING:
        if (now - last >= PERIOD_PING) {
            last = now;
            send_ping();
        }
        return;
    }
}

// data must be at least len + 8 bytes long, as we prepend the SNAP header in place
int net_send_to_gateway(unsigned char *data, unsigned int len) {
    if (wpa_state != WPA_DONE || !have_gw)
        return -1;
    if (len > MTU - 8)
        return -1;

    memmove(data + LLC_SNAP_LEN, data, len);
    put_snap(data, ETHERTYPE_IPV4);
    return wifi_ccmp_tx(gw_mac, data, len + LLC_SNAP_LEN);
}
