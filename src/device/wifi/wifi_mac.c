#include "reg_util.h"

#define MAC_EVENT_MASK 0x00030000

void init_wifi_mac(void) {
    WRITE_REG(0x3ff20c18, 0);
    WRITE_REG(0x3ff20c24, 0xffffffff);
    WRITE_REG(0x3ff20800, MAC_EVENT_MASK);
    WRITE_REG(0x3ff20804, MAC_EVENT_MASK);
    WRITE_REG(0x3ff20404, 0xbbbbbbbb);
    WRITE_REG(0x3ff20408, 0xbbbbbbbb);
    WRITE_REG_MASK(0x3ff2006c, 0x707);
    WRITE_REG(0x3ff2006c, READ_REG(0x3ff2006c) & 0xffffefff);
    WRITE_REG_MASK(0x3ff20178, 2);
}
