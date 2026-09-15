#include "def.h"
#include "uart.h"
#include "string.h"
#include "wifi_ccmp.h"
#include "wifi_wpa.h"
#include "net.h"
#include "ipv4.h"
#include "udp.h"
#include "proc.h"
#include "timer.h"

#define MTU (1518)

#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPV4 0x0800

#define LLC_SNAP_LEN 8

#define ARP_HTYPE_ETHERNET 1
#define ARP_OPER_REQUEST 1
#define ARP_OPER_REPLY   2

#define ON_SUBNET(ip) ((((ip).word ^ local_ip.word) & net_mask.word) == 0)

extern unsigned char wifi_mac_addr[6];
extern volatile int wpa_state;
extern struct ipv4_addr local_ip;
extern struct ipv4_addr gw_ip;
extern struct ipv4_addr net_mask;

static const u8 bcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static int arp_cond;
static int arp_cache_mutex;

#define ARP_CACHE_SIZE 4

static struct {
    struct ipv4_addr ip;
    u8 mac[6];
} arp_cache[ARP_CACHE_SIZE];
static int arp_cache_next;

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

#define ARP_RETRY_TICKS (TICKS_PER_SEC / 2)
#define ARP_RETRIES 6

void net_init(void) {
    arp_cond = cond_create();
    arp_cache_mutex = mutex_create();
}

static void arp_cache_store(struct ipv4_addr ip, const u8 *mac) {
    mutex_lock(arp_cache_mutex);
    int i;
    for (i = 0; i < arp_cache_next; i++)
        if (arp_cache[i].ip.word == ip.word)
            break;
    if (i == ARP_CACHE_SIZE)
        i = ARP_CACHE_SIZE - 1;
    arp_cache[i].ip = ip;
    memcpy(arp_cache[i].mac, mac, 6);
    if (i == arp_cache_next)
        arp_cache_next++;
    cond_broadcast(arp_cond);
    mutex_unlock(arp_cache_mutex);
}

static int arp_cache_lookup(struct ipv4_addr ip, u8 *mac) {
    if (ip.word == IPV4_BROADCAST) {
        memcpy(mac, bcast, 6);
        return 0;
    }
    for (int i = 0; i < arp_cache_next; i++) {
        if (arp_cache[i].ip.word == ip.word) {
            memcpy(mac, arp_cache[i].mac, 6);
            return 0;
        }
    }
    return -1;
}

static void put_snap(u8 *b, unsigned int ethertype) {
    b[0] = 0xaa; b[1] = 0xaa; b[2] = 0x03;
    b[3] = 0x00; b[4] = 0x00; b[5] = 0x00;
    b[6] = ethertype >> 8; b[7] = ethertype & 0xff;
}

int net_tx(const unsigned char *mac, unsigned char *pkt, unsigned int len) {
    put_snap(pkt - LLC_SNAP_LEN, ETHERTYPE_IPV4);
    return wifi_ccmp_tx(mac, pkt - LLC_SNAP_LEN, len + LLC_SNAP_LEN);
}

static void send_arp_request(struct ipv4_addr target_ip) {
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
    a->target_ip = target_ip;
    wifi_ccmp_tx(bcast, p, sizeof(p));
}

static void arp_recv(u8 *payload, unsigned int len) {
    if (len < sizeof(struct arp_pkt))
        return;
    struct arp_pkt *a = (struct arp_pkt *) payload;
    arp_cache_store(a->sender_ip, a->sender_mac);
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

void net_recv(unsigned char *llc, unsigned int len, const unsigned char *sa) {
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
            ipv4_enqueue(ip, total_len, sa);
            break;
        }
        default:
            break;
    }
}

int net_send(struct ipv4_addr dst, unsigned char *pkt, unsigned int len) {
    if (wpa_state != WPA_DONE)
        return -1;
    if (len > MTU - 8)
        return -1;

    memmove(pkt + LLC_SNAP_LEN, pkt, len);
    put_snap(pkt, ETHERTYPE_IPV4);
    struct ipv4_addr next = (ON_SUBNET(dst) || dst.word == IPV4_BROADCAST) ? dst : gw_ip;

    u8 mac[6];
    unsigned int tries = 0;
    mutex_lock(arp_cache_mutex);
    while (arp_cache_lookup(next, mac) < 0) {
        if (tries == ARP_RETRIES) {
            mutex_unlock(arp_cache_mutex);
            return -1;
        }
        mutex_unlock(arp_cache_mutex);
        send_arp_request(next);
        mutex_lock(arp_cache_mutex);
        if (cond_timedwait(arp_cond, arp_cache_mutex, ARP_RETRY_TICKS) < 0)
            tries++;
    }
    mutex_unlock(arp_cache_mutex);
    return wifi_ccmp_tx(mac, pkt, len + LLC_SNAP_LEN);
}
