#include "reg_util.h"
#include "wifi_regs.h"

void init_wifi_iomux(void) {
    WRITE_REG_RMW(PERIPHS_IO_MUX_MTCK_U, 0xfffffe0f, 0x30);
    WRITE_REG_RMW(PERIPHS_IO_MUX_MTMS_U, 0xfffffe0f, 0x30);
    WRITE_REG_RMW(PERIPHS_IO_MUX_GPIO0_U, 0xfffffecf, 0x30);
}
