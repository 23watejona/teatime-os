#include "reg_util.h"
#include "wifi_regs.h"

extern void kprintf_uart(const char *, ...);

unsigned char wifi_mac_addr[6];

static unsigned char crc8(const unsigned char *p, unsigned int len) {
    unsigned char c = 0;
    while (len--) {
        c ^= *p++;
        for (int i = 0; i < 8; i++)
            c = (c >> 1) ^ (0x8c & -(c & 1));
    }
    return c;
}

void init_wifi_mac_addr(void) {
    unsigned int data0 = READ_REG(EFUSE_DATA0_REG);
    unsigned int data1 = READ_REG(EFUSE_DATA1_REG);
    unsigned int data2 = READ_REG(EFUSE_DATA2_REG);
    unsigned int data3 = READ_REG(EFUSE_DATA3_REG);

    unsigned char mac[6];
    mac[3] = (data1 >> 8) & 0xff;
    mac[4] = data1 & 0xff;
    mac[5] = (data0 >> 24) & 0xff;

    int valid = 0;
    if (data2 & EFUSE_IS_48BITS_MAC) {
        mac[0] = (data3 >> 16) & 0xff;
        mac[1] = (data3 >> 8) & 0xff;
        mac[2] = data3 & 0xff;
        unsigned char check[3] = { mac[2], mac[1], mac[0] };
        if (crc8(check, 3) == (data2 >> 24))
            valid = 1;
    }
    if (!valid) {
        mac[0] = 0x18;
        mac[1] = 0xfe;
        mac[2] = 0x34;
        mac[3] = 0;
        mac[4] = 0;
        mac[5] = 0;
    }

    for (int i = 0; i < 6; i++)
        wifi_mac_addr[i] = mac[i];

    kprintf_uart("MAC: %x:%x:%x:%x:%x:%x\n",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    unsigned int lo = mac[0] | (mac[1] << 8) | (mac[2] << 16) | (mac[3] << 24);
    unsigned int hi = mac[4] | (mac[5] << 8);
    WRITE_REG(MAC_ADDR_LO(0), lo);
    WRITE_REG(MAC_ADDR_HI(0), hi);
}
