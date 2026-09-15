#ifndef WIFI_REGS_H
#define WIFI_REGS_H

#define DPORT_CTL_REG        0x3ff00014
#define DPORT_CTL_DOUBLE_CLK 0x00000001
#define DPORT_CLK_EN         0x3ff00018
#define DPORT_WIFI_CLK_EN    0x00100000
#define EFUSE_DATA0_REG      0x3ff00050
#define EFUSE_DATA1_REG      0x3ff00054
#define EFUSE_DATA2_REG      0x3ff00058
#define EFUSE_DATA3_REG      0x3ff0005c
#define EFUSE_IS_48BITS_MAC  (1 << 12)

#define PERIPHS_IO_MUX_MTCK_U  0x60000808
#define PERIPHS_IO_MUX_MTMS_U  0x6000080c
#define PERIPHS_IO_MUX_GPIO0_U 0x60000834

#define MAC_DMA_RX_CTRL             0x3ff20000
#define MAC_TX_CTRL                 0x3ff20004
#define MAC_TX_ENABLE               0x80000000
#define MAC_DMA_RX_HEAD             0x3ff20008
#define MAC_DMA_RX_CTRL_HEAD        0x3ff2000c
#define MAC_DMA_RX_CURRENT          0x3ff2001c
// rx filter: bits 0-2 enable the address-match filters (clear accepts every frame), bit 4 is held while scanning or associated
#define MAC_RX_FILTER               0x3ff2006c
#define MAC_DMA_RX_WINDOW_END       0x3ff2007c
#define MAC_DMA_RX_WINDOW_BASE      0x3ff20080
#define MAC_DMA_RX_CTRL_WINDOW_END  0x3ff20084
#define MAC_DMA_RX_CTRL_WINDOW_BASE 0x3ff20088

// crypto config, one register per direction: low byte cipher select, bit 16 engine enable, bits 24-25 pass frames through undecrypted
#define MAC_CRYPTO_CIPHER    0x3ff20800
#define MAC_CRYPTO_CONF      0x3ff20804
#define MAC_CRYPTO_OFF       0x00030000
#define MAC_CIPHER_CCMP      0x00030103
#define MAC_KEY_ENABLE       0x3ff2080c
#define MAC_KEY_SLOT(s)      (0x3ff21400 + 0x28 * (s))
#define MAC_KEY_ADDR(s)      MAC_KEY_SLOT(s)
#define MAC_KEY_FLAGS(s)     (MAC_KEY_SLOT(s) + 4)
#define MAC_KEY_MATERIAL(s, w) (MAC_KEY_SLOT(s) + 8 + 4 * (w))
// key flag word: bits 18-20 key type (3 for a station key), bits 21-23 cipher code (2 = ccmp), bit 24 key id, bit 31 marks a unicast key
#define MAC_KEY_CCMP         0x004c0000
#define MAC_KEY_ID_SHIFT     24

#define MAC_CTRL             0x3ff20c14
#define MAC_INT_ENABLE       0x3ff20c18
#define MAC_INT_EVENT        0x3ff20c20
#define MAC_INT_CLEAR        0x3ff20c24
#define MAC_INT_TX_DONE      0x00080000
#define WDEV_INTEREST_EVENT  0x2c880300
#define WDEV_SNIFFER_EVENT   0x0000000c

// two address-match units, each with an address and a byte mask; bit 16 of the mask high word enables the unit
#define MAC_BSSID_LO(u)      (0x3ff20c28 + 8 * (u))
#define MAC_BSSID_HI(u)      (0x3ff20c2c + 8 * (u))
#define MAC_BSSID_MASK_LO(u) (0x3ff20c38 + 8 * (u))
#define MAC_BSSID_MASK_HI(u) (0x3ff20c3c + 8 * (u))
#define MAC_ADDR_LO(u)       (0x3ff20c48 + 8 * (u))
#define MAC_ADDR_HI(u)       (0x3ff20c4c + 8 * (u))
#define MAC_ADDR_MASK_LO(u)  (0x3ff20c58 + 8 * (u))
#define MAC_ADDR_MASK_HI(u)  (0x3ff20c5c + 8 * (u))
#define MAC_ADDR_MATCH_ENABLE 0x00010000

#define MAC_PHY_CTRL         0x3ff20c70
#define MAC_PHY_RF_UP        0x00000002
#define MAC_TX_STATUS        0x3ff20c84
// rx option bit 18 passes protected frames to the dma (sniffer raw delivery, but also needed for decrypted delivery); tx option bit 0 is the automatic ack
#define MAC_RX_OPTION        0x3ff20c88
#define MAC_RXTX_OPTION      0x3ff20c90
#define MAC_TX_OPTION        0x3ff20c94
#define MAC_PHY_CONF         0x3ff20e08

#define MAC_TXQ(q)           (0x3ff20dc0 - 0x18 * (q))
#define TXQ_LEN              0x00
#define TXQ_DESC             0x04
#define TXQ_RATE             0x08
#define TXQ_DURATION         0x10
#define TXQ_LIFETIME         0x14

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

// rfpll control: bits 20-23 latch the pll while its divider is reprogrammed, bits 16-17 hold its output across sleep
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
// rx gain force: lna code in the low byte, vga code from bit 2, latch in bit 0
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
// noise floor: floor value in bits 0-8, channel index in bits 9-11, cca threshold in bits 12-19, measured floor read back from bits 20-31
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
