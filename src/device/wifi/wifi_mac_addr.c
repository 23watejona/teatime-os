#include "reg_util.h"

extern void kprintf_uart(const char *, ...);

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
    unsigned int r1 = READ_REG(0x3ff00050);
    unsigned int r2 = READ_REG(0x3ff00054);
    unsigned int r3 = READ_REG(0x3ff00058);
    unsigned int r4 = READ_REG(0x3ff0005c);

    unsigned char mac[6];
    mac[3] = (r2 >> 8) & 0xff;
    mac[4] = r2 & 0xff;
    mac[5] = (r1 >> 24) & 0xff;

    int valid = 0;
    if (r3 & (1 << 12)) {
        mac[0] = (r4 >> 16) & 0xff;
        mac[1] = (r4 >> 8) & 0xff;
        mac[2] = r4 & 0xff;
        unsigned char check[3] = { mac[2], mac[1], mac[0] };
        if (crc8(check, 3) == ((r3 >> 24) & 0xff))
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

    kprintf_uart("MAC: %x:%x:%x:%x:%x:%x\n",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    unsigned int lo = mac[0] | (mac[1] << 8) | (mac[2] << 16) | (mac[3] << 24);
    unsigned int hi = mac[4] | (mac[5] << 8);
    WRITE_REG(0x3ff20c48, lo);
    WRITE_REG(0x3ff20c4c, hi);
}
