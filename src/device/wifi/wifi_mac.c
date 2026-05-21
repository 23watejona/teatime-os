#include "reg_util.h"

#define MAC_EVENT_MASK 0x00030000

/* MAC event-interest mask for 0x3ff20c18. */
#define WDEV_INTEREST_EVENT 0x2c880300

/* crypto key table + option-init omitted. */
void init_wifi_mac(void) {
    WRITE_REG(0x3ff20c18, 0);
    WRITE_REG(0x3ff20c24, 0xffffffff);
    WRITE_REG(0x3ff20800, MAC_EVENT_MASK);
    WRITE_REG(0x3ff20804, MAC_EVENT_MASK);
    WRITE_REG(0x3ff20808, 0);
    WRITE_REG(0x3ff20400, 0x76503210);
    WRITE_REG(0x3ff20404, 0xbbbbbbbb);
    WRITE_REG(0x3ff20408, 0xbbbbbbbb);
    WRITE_REG_MASK(0x3ff2006c, 0x707);
    WRITE_REG_UNMASK(0x3ff2006c, 0x00000010);
    WRITE_REG_UNMASK(0x3ff2006c, 0x00001000);
    WRITE_REG_UNMASK(0x3ff2006c, 0x00000007);
    WRITE_REG(0x3ff20c18, WDEV_INTEREST_EVENT);
}

/* Post-init RX open: drop BSSID filter, set MAC RX-enable bit31 of 0x3ff20004. */
void wifi_mac_rx_enable(void) {
    WRITE_REG(0x3ff20c3c, 0x00010000);
    WRITE_REG(0x3ff20c44, 0x00000000);
    WRITE_REG_MASK(0x3ff20178, 2);
    WRITE_REG_MASK(0x3ff20004, 0x80000000);
}
