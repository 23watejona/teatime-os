#include "def.h"
#include "string.h"
#include "uart.h"
#include "ipv4.h"
#include "udp.h"
#include "dhcp.h"
#include "dev.h"
#include "proc.h"
#include "timer.h"

#define XID 0x7ea7ea00
#define REPLY_TICKS (2 * TICKS_PER_SEC)
#define RETRIES 8

extern unsigned char wifi_mac_addr[6];
extern struct ipv4_addr local_ip;
extern struct ipv4_addr gw_ip;
extern struct ipv4_addr net_mask;

static u8 txbuf[sizeof(struct udp_datagram) + DHCP_MSG_LEN] __attribute__((aligned(4)));
static u8 rxbuf[sizeof(struct udp_datagram) + UDP_DATAGRAM_MAX] __attribute__((aligned(4)));

void dhcp_proc(void) {
    int fd = open("udp", 0);
    if (fd < 0 || control(fd, UDP_BIND, DHCP_CLIENT_PORT) < 0
        || control(fd, UDP_TIMEOUT, REPLY_TICKS) < 0)
        return;

    struct udp_datagram *tx = (struct udp_datagram *) txbuf;
    struct dhcp_msg *req = (struct dhcp_msg *) tx->data;
    struct dhcp_opts *opts = (struct dhcp_opts *) req->options;
    struct udp_datagram *rx = (struct udp_datagram *) rxbuf;
    const struct dhcp_msg *reply = (const struct dhcp_msg *) rx->data;
    unsigned int xid = htonl(XID);
    unsigned int i;

    tx->addr.word = IPV4_BROADCAST;
    tx->port = DHCP_SERVER_PORT;
    tx->len = DHCP_MSG_LEN;
    req->op = DHCP_BOOTREQUEST;
    req->htype = DHCP_HTYPE_ETHERNET;
    req->hlen = sizeof(wifi_mac_addr);
    req->xid = xid;
    memcpy(req->chaddr, wifi_mac_addr, sizeof(wifi_mac_addr));
    req->cookie = htonl(DHCP_COOKIE);
    opts->type_code = DHCP_OPT_MSG_TYPE;
    opts->type_len = sizeof(opts->type);
    opts->param_code = DHCP_OPT_PARAM_REQUEST;
    opts->param_len = sizeof(opts->params);
    opts->params[0] = DHCP_OPT_SUBNET_MASK;
    opts->params[1] = DHCP_OPT_ROUTER;

    // the link may still be down, so a failed write just waits out the read timeout
    opts->type = DHCP_DISCOVER;
    opts->req_code = DHCP_OPT_END;
    for (i = 0; i < RETRIES; i++) {
        write(fd, txbuf, sizeof(txbuf));
        if (read(fd, rxbuf, sizeof(rxbuf)) < 0)
            continue;
        if (rx->len >= DHCP_MSG_LEN && reply->xid == xid
            && reply->options[0] == DHCP_OPT_MSG_TYPE && reply->options[2] == DHCP_OFFER)
            break;
    }
    if (i == RETRIES)
        return;
    kprintf_uart("dhcp: offer %u.%u.%u.%u\n", reply->yiaddr.bytes[0], reply->yiaddr.bytes[1],
                 reply->yiaddr.bytes[2], reply->yiaddr.bytes[3]);

    // no relay on this lan, so the offer's source is the server
    opts->type = DHCP_REQUEST;
    opts->req_code = DHCP_OPT_REQUESTED_IP;
    opts->req_len = sizeof(opts->req);
    memcpy(opts->req, reply->yiaddr.bytes, sizeof(opts->req));
    opts->sid_code = DHCP_OPT_SERVER_ID;
    opts->sid_len = sizeof(opts->sid);
    memcpy(opts->sid, rx->addr.bytes, sizeof(opts->sid));
    opts->end = DHCP_OPT_END;
    for (i = 0; i < RETRIES; i++) {
        write(fd, txbuf, sizeof(txbuf));
        if (read(fd, rxbuf, sizeof(rxbuf)) < 0)
            continue;
        if (rx->len >= DHCP_MSG_LEN && reply->xid == xid
            && reply->options[0] == DHCP_OPT_MSG_TYPE && reply->options[2] == DHCP_ACK)
            break;
    }
    if (i == RETRIES)
        return;

    const u8 *o = reply->options;
    const u8 *end = (const u8 *) reply + rx->len;
    while (o + 2 <= end && *o != DHCP_OPT_END) {
        if (*o == DHCP_OPT_SUBNET_MASK)
            memcpy(net_mask.bytes, o + 2, sizeof(net_mask.bytes));
        if (*o == DHCP_OPT_ROUTER)
            memcpy(gw_ip.bytes, o + 2, sizeof(gw_ip.bytes));
        o += 2 + o[1];
    }
    local_ip = reply->yiaddr;
    close(fd);
    kprintf_uart("dhcp: ip %u.%u.%u.%u gw %u.%u.%u.%u mask %u.%u.%u.%u\n",
                 local_ip.bytes[0], local_ip.bytes[1], local_ip.bytes[2], local_ip.bytes[3],
                 gw_ip.bytes[0], gw_ip.bytes[1], gw_ip.bytes[2], gw_ip.bytes[3],
                 net_mask.bytes[0], net_mask.bytes[1], net_mask.bytes[2], net_mask.bytes[3]);
}
