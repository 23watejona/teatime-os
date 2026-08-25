#include "def.h"
#include "reg_util.h"
#include "wdt.h"

// stage timeouts are 2^n * 0.8 ms
#define WDT_CTL   0x60000900
#define WDT_OP    0x60000904
#define WDT_OP_ND 0x60000908
#define WDT_RST   0x60000914

IRAM_ATTR void wdt_feed(void) {
    WRITE_REG(WDT_RST, 0x73);
}

void wdt_enable(void) {
    WRITE_REG_UNMASK(WDT_CTL, 1);
    WRITE_REG(WDT_OP, 12);
    WRITE_REG(WDT_OP_ND, 11);
    WRITE_REG_RMW(WDT_CTL, 0xffffffc1, 0x38);
    WRITE_REG_MASK(WDT_CTL, 1);
    wdt_feed();
}
void system_reboot(void) {
    WRITE_REG_UNMASK(WDT_CTL, 2);
    WRITE_REG(WDT_OP, 1);
    WRITE_REG(WDT_OP_ND, 1);
    WRITE_REG_MASK(WDT_CTL, 0x38);
    WRITE_REG_UNMASK(WDT_CTL, 6);
    WRITE_REG_MASK(WDT_CTL, 1);
    for (;;)
        ;
}
