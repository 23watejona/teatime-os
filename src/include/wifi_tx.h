#ifndef WIFI_TX_H
#define WIFI_TX_H

#define FC_DATA 0x08
#define FC_TO_DS 0x01
#define FC_PROTECTED 0x40
#define CCMP_EXT_IV 0x20
#define CCMP_MIC_LEN 8

struct mac_header {
    unsigned char frame_control[2];
    unsigned char duration[2];
    unsigned char addr1[6];
    unsigned char addr2[6];
    unsigned char addr3[6];
    unsigned short sequence_control;
} __attribute__((packed));

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

void wifi_tx_init(void);

/* One 802.11 frame, no FCS. Fills the sequence number and, if Protected,
   the CCMP header. Blocks for TX-done: 0 sent, -1 timeout or too long. */
int wifi_tx_frame(const unsigned char *frame, unsigned int len);

/* Send a wildcard-SSID probe request to the AP. */
int wifi_tx_probe_req(void);

#endif
