#include "reg_util.h"

void init_wifi_iomux(void) {
    WRITE_REG_RMW(0x60000808, 0xfffffe0f, 0x30);
    WRITE_REG_RMW(0x6000080c, 0xfffffe0f, 0x30);
    WRITE_REG_RMW(0x60000834, 0xfffffecf, 0x30);
}
