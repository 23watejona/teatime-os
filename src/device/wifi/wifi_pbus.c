#include "reg_util.h"
#include "wifi_regs.h"

void init_wifi_pbus(void) {
    WRITE_REG_RMW(PBUS_CFG, 0xf01fffff, 0x01800000);
    WRITE_REG_RMW(PBUS_CFG_MIRROR, 0xf01fffff, 0x01800000);
    WRITE_REG_RMW(PBUS_CTRL, 0x1fffffff, 0xc0000000);
    WRITE_REG_RMW(PBUS_CFG, 0xffe03fff, 0x001f4000);
    WRITE_REG_RMW(PBUS_STATUS, 0xff00ffff, 0x00ab0000);
    WRITE_REG_RMW(RFPLL_CTRL, 0xffff00ff, 0x00000100);
    WRITE_REG_RMW(PBUS_CFG, 0xffffff80, 0x00000047);
}
