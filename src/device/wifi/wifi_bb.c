#include "reg_util.h"
#include "wifi_regs.h"
#include "wifi_rxgain.h"

extern void pbus_debug_mode(void);
extern void pbus_work_mode(void);
extern void pbus_force(unsigned int reg, unsigned int width, unsigned int val);
extern void rx_max_gain_digital(unsigned int ch, int level);
extern unsigned int g_wifi_channel;

void init_wifi_bb(void) {
    WRITE_REG_MASK(BB_RX_CTRL, 0x00001400);
    WRITE_REG_MASK(0x60009b0c, 0x10000000);
    WRITE_REG_RMW(0x60009d40, 0x807fffff, 0x10000000);
    WRITE_REG(0x60009b6c, 0x0914bc81);
    WRITE_REG(0x60009b68, 0x5ac64198);
    WRITE_REG_UNMASK(0x60009b50, 0x80000000);
    asm volatile("memw");
    WRITE_REG(0x60009d18, 400);
    WRITE_REG_RMW(0x600098ec, 0xfc00ffff, 0x01900000);
    WRITE_REG_UNMASK(0x60009988, 0x04000000);
    WRITE_REG_RMW(BB_RX_MAX_GAIN, 0xffffff80, 0x46);
    WRITE_REG_RMW(0x60009b28, 0x00ffffff, 0x18000000);
    WRITE_REG_UNMASK(BB_RX_GAIN_FORCE, 0x7f000000);
    WRITE_REG_RMW(0x60009b44, 0xffffff80, 0x26);
    WRITE_REG_RMW(0x60009d70, 0xffffffc0, 0x11);
    WRITE_REG_MASK(0x600098a0, 0xc0000000);
    WRITE_REG_RMW(BB_NOISE_FLOOR, 0xfff00fff, 0x00022000);
    WRITE_REG_RMW(BB_NOISE_FLOOR, 0xfffff000, 0x00000fa6);
    WRITE_REG_RMW(0x60009b5c, 0xffc00000, 0x00385854);
    WRITE_REG_RMW(0x60009b50, 0xf00fff00, 0x0b2000e6);
    asm volatile("memw");
    WRITE_REG(0x60009d18, 0x80);
    WRITE_REG_MASK(0x60009d10, 4);
    WRITE_REG_RMW(0x60009d70, 0xdffff03f, 0x20000c40);
    WRITE_REG_RMW(0x60009d24, 0xff80ffff, 0x00130000);
    WRITE_REG_RMW(0x60009b58, 0xfffff03f, 0x00000d80);
    WRITE_REG_RMW(0x60009d4c, 0xfc000000, 0x03fe0124);
    WRITE_REG_RMW(0x60009d20, 0x0fffffff, 0xb0000000);
    WRITE_REG_MASK(0x60009988, 0x04000000);
    WRITE_REG_RMW(MAC_PHY_CTRL, 0xff0bffff, 0x00240000);
    WRITE_REG_UNMASK(0x60009d44, 0x00400000);

    rx_max_gain_digital(g_wifi_channel, 0);

    WRITE_REG_RMW(0x60009c28, 0xfffe03ff, 0);
    WRITE_REG_RMW(0x60009d24, 0xffffff01, 0);

    WRITE_REG_RMW(0x60009838, 0xffffffcf, 0x20);
    asm volatile("memw");
    WRITE_REG(0x60009c48, 0x00800083);
    WRITE_REG_MASK(0x60009c4c, 6);
    WRITE_REG(0x60009d1c, 0x00000fff);
    WRITE_REG_MASK(0x60009d1c, 0x00000fff);
    WRITE_REG_UNMASK(BB_TX_CAL, 0x00000800);
    WRITE_REG_RMW(0x3ff00024, 0xfffffff9, 2);
    WRITE_REG_UNMASK(0x600005c0, 0x00000001);

    pbus_debug_mode();
    pbus_force(1, 1, 0xc);
    pbus_work_mode();
    rx_gain_init(3);
    rx_filter_select(3);
}
