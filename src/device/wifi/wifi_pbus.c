#include "reg_util.h"

void init_wifi_pbus(void) {
    WRITE_REG_RMW(0x60000598, 0xf01fffff, 0x01800000);
    WRITE_REG_RMW(0x6000059c, 0xf01fffff, 0x01800000);
    WRITE_REG_RMW(0x60000594, 0x1fffffff, 0xc0000000);
    WRITE_REG_RMW(0x60000598, 0xffe03fff, 0x00401f00);
    WRITE_REG_RMW(0x600005a0, 0xff00ffff, 0x00ab0000);
    WRITE_REG_RMW(0x600005c8, 0xffff00ff, 0x00000100);
    WRITE_REG_RMW(0x60000598, 0xffffff80, 0x00000047);
}
