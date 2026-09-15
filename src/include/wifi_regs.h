#ifndef WIFI_REGS_H
#define WIFI_REGS_H

#define DPORT_CTL_REG        0x3ff00014
#define DPORT_CTL_DOUBLE_CLK 0x00000001
#define DPORT_CLK_EN         0x3ff00018
#define DPORT_WIFI_CLK_EN    0x00100000
#define EFUSE_DATA2_REG      0x3ff00058

#define MAC_CTRL             0x3ff20c14
#define MAC_PHY_CTRL         0x3ff20c70
#define MAC_PHY_RF_UP        0x00000002

#define RF_TX_DC_REG(i)      (0x60000404u + 4u * ((i) >> 1))
#define RF_TXPWR_REGS        24
#define RF_TXPWR_REG(i)      (0x60000504u + 4u * (i))
#define RF_RX_GAIN_EXT       0x60000590
#define RF_RX_GAIN_EXT_DIG   0x00000010

#define PBUS_CTRL              0x60000594
#define PBUS_DEBUG_MODE        0x00000001
#define PBUS_FORCE_STROBE      0x00000002
#define PBUS_FORCE_REG_SHIFT   2
#define PBUS_FORCE_VAL_SHIFT   5
#define PBUS_FORCE_WIDTH_SHIFT 14
#define PBUS_CFG               0x60000598
#define PBUS_CFG_MIRROR        0x6000059c
#define PBUS_STATUS            0x600005a0
#define PBUS_READY             0x40000000
#define PBUS_BUSY              0x80000000
#define PBUS_READ_WINDOW       0x600005a4

#define RFPLL_CTRL           0x600005c8
#define RFPLL_LATCH          0x00f00000
#define RF_CAL_MODE          0x600005e8

#define RF_I2C_HOST(h)       (0x60000d00 + (h) * 4)
#define RF_I2C_START         (1u << 24)
#define RF_I2C_BUSY          (1u << 25)
#define BBPLL_CTRL           0x60000d40

#define BB_RX_RSSI           0x60009824
#define BB_TX_CAL            0x60009a28
#define BB_DIG_RX            0x60009a2c
#define BB_DIG_RX_EN         0x00080000
#define BB_RX_GAIN_FORCE     0x60009a34
#define BB_RX_GAIN_LATCH     0x00000001
#define BB_RX_GAIN_WINDOW    0x60009a68
#define BB_AGC_CTRL          0x60009b00
#define BB_AGC_OFF           0x10000000
#define BB_SLEEP             0x60009b08
#define BB_SLEEP_RX          0x08000000
#define BB_CHAN_FREQ         0x60009b14
#define BB_RX_MAX_GAIN       0x60009b48
#define BB_RX_CTRL           0x60009b60
#define BB_RX_RESET          0x00000001
#define BB_NOISE_MEAS        0x00000002
#define BB_NOISE_FLOOR       0x60009b64
#define BB_RX_FILTER         0x60009c04
#define BB_ANT_SWITCH_LO     0x60009d60
#define BB_ANT_SWITCH_HI     0x60009d64
#define BB_RX_CHAN_COMP      0x60009d68
#define BB_ANALOG_DEFAULT    0x60009d74
#define BB_RX_GAIN_TABLE     0x60009e00

#define I2C_RFPLL            98
#define I2C_RFPLL_SDM        99
#define I2C_RX_GAIN          100
#define I2C_TX               107
#define I2C_SAR              108
#define I2C_BB               119

#endif
