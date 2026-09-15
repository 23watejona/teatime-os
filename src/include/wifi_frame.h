#ifndef WIFI_FRAME_H
#define WIFI_FRAME_H

// the mac puts its rx control header in front of every received frame, so every parser skips it first; byte 0 is the rssi
#define RXCTRL_LEN    12
#define MAX_FRAME_LEN 1600

#define FC_TYPE(fc0)     (((fc0) >> 2) & 3)
#define FC_SUBTYPE(fc0)  ((fc0) >> 4)
#define FC_TYPE_SHIFT    2
#define FC_SUBTYPE_SHIFT 4
#define FC_TYPE_MGMT     0
#define FC_TYPE_DATA     2
#define FC_DATA          (FC_TYPE_DATA << FC_TYPE_SHIFT)
#define SUBTYPE_QOS      8
#define FC_TO_DS         0x01
#define FC_PROTECTED     0x40

#define MGMT_ASSOC_REQ   0
#define MGMT_ASSOC_RESP  1
#define MGMT_PROBE_REQ   4
#define MGMT_PROBE_RESP  5
#define MGMT_BEACON      8
#define MGMT_DISASSOC    10
#define MGMT_AUTH        11
#define MGMT_DEAUTH      12

struct mac_header {
    unsigned char frame_control[2];
    unsigned char duration[2];
    unsigned char addr1[6];
    unsigned char addr2[6];
    unsigned char addr3[6];
    unsigned short sequence_control;
} __attribute__((packed));

#define MAC_HDR_LEN  24
#define QOS_CTRL_LEN 2
_Static_assert(sizeof(struct mac_header) == MAC_HDR_LEN, "mac_header must be 24 bytes");

struct auth_body {
    unsigned short algorithm;
    unsigned short sequence;
    unsigned short status;
} __attribute__((packed));

#define AUTH_ALGO_OPEN    0
#define AUTH_SEQ_REQUEST  1
#define AUTH_SEQ_RESPONSE 2

struct assoc_req_body {
    unsigned short capability;
    unsigned short listen_interval;
} __attribute__((packed));

struct assoc_resp_body {
    unsigned short capability;
    unsigned short status;
    unsigned short aid;
} __attribute__((packed));

struct reason_body {
    unsigned short reason;
} __attribute__((packed));

#define CAP_ESS            0x0001
#define CAP_PRIVACY        0x0010
#define CAP_SHORT_PREAMBLE 0x0020
#define CAP_SHORT_SLOT     0x0400
#define AID_MASK           0x3fff

#define BEACON_FIXED 12

#define IE_HDR_LEN         2
#define IE_SSID            0
#define IE_SUPPORTED_RATES 1
#define IE_DS_PARAMS       3
#define IE_RSN             48

#define RATE_BASIC     0x80
#define RATE_KBPS(k)   ((k) / 500)

#define RSN_VERSION        1
#define RSN_VERSION_LEN    2
#define RSN_COUNT_LEN      2
#define RSN_SUITE_LEN      4
#define RSN_SUITE(type)    0x00, 0x0f, 0xac, (type)
#define RSN_CIPHER_TKIP    2
#define RSN_CIPHER_CCMP    4
#define RSN_AKM_PSK        2
#define RSN_AKM_PSK_SHA256 6
#define RSN_AKM_SAE        8

struct llc_snap {
    unsigned char dsap;
    unsigned char ssap;
    unsigned char control;
    unsigned char oui[3];
    unsigned char ethertype[2];
} __attribute__((packed));

#define LLC_SNAP_LEN    8
#define LLC_SAP_SNAP    0xaa
#define LLC_CONTROL_UI  0x03
#define ETHERTYPE_EAPOL 0x888e
_Static_assert(sizeof(struct llc_snap) == LLC_SNAP_LEN, "llc_snap must be 8 bytes");

struct ccmp_header {
    unsigned char pn0;
    unsigned char pn1;
    unsigned char reserved;
    unsigned char key_id;
    unsigned char pn2;
    unsigned char pn3;
    unsigned char pn4;
    unsigned char pn5;
};

#define CCMP_HDR_LEN 8
#define CCMP_MIC_LEN 8
#define CCMP_EXT_IV  0x20
_Static_assert(sizeof(struct ccmp_header) == CCMP_HDR_LEN, "ccmp_header must be 8 bytes");

#endif
