#include "reg_util.h"
#include "wait.h"
#include "timer.h"
#include "wifi_regs.h"

extern void init_wifi_dma(void);
extern void wifi_rf_on(void);
extern void rf_off(void);

// mac feature options, the phy interface config, the two mac clock dividers and the mac enable bits
static void mac_options_init(void) {
    // rx feature options: which frame classes and errors reach the dma; the individual bits are not decoded
    WRITE_REG_MASK(MAC_RX_OPTION, 0x8084a000);
    WRITE_REG_RMW(MAC_RX_OPTION, 0xffdfbff7, 0);
    WRITE_REG_MASK(MAC_RXTX_OPTION, 0x00000008);
    // mac transmit enable and one more tx option bit
    WRITE_REG_MASK(MAC_TX_OPTION, 0x00000003);
    // phy interface mode field cleared to its default
    WRITE_REG_RMW(MAC_PHY_CONF, 0xffffff0f, 0);
    // the first mac clock field is raised by a fixed step rather than set outright, so it stays relative to its reset value
    unsigned int v = READ_REG(0x3ff20c68);
    WRITE_REG_RMW(0x3ff20c68, 0xff00ffff, (((v >> 16) + 18) & 0xff) << 16);
    WRITE_REG_RMW(0x3ff20c6c, 0xffffff00, 0x16);
    WRITE_REG_RMW(0x3ff20c6c, 0xffff00ff, 0x1600);
    // the low field is the mac mode (this value active, 2 for sleep), bit 30 is dropped for sleep and bit 31 is the mac enable that stays on
    WRITE_REG_RMW(MAC_CTRL, 0xfffff000, 0x0f0);
    WRITE_REG_MASK(MAC_CTRL, 0x80000000);
    WRITE_REG_MASK(MAC_CTRL, 0x40000000);
}

// two placeholder slots with a broadcast address and an all-ones key, so the crypto engine has valid entries before any real key is installed
static void key_table_init(void) {
    // flag word: key type 3, cipher code 6 (none), key id 0 in the first slot and 1 in the second, so both key ids resolve
    WRITE_REG(MAC_KEY_ADDR(0), 0xffffffff);
    WRITE_REG(MAC_KEY_FLAGS(0), 0x00ccffff);
    WRITE_REG(MAC_KEY_MATERIAL(0, 0), 0xffffffff);
    WRITE_REG_RMW(MAC_KEY_MATERIAL(0, 1), 0xffff0000, 0x0000ffff);
    WRITE_REG_MASK(MAC_KEY_ENABLE, 1 << 0);
    WRITE_REG(MAC_KEY_ADDR(1), 0xffffffff);
    WRITE_REG(MAC_KEY_FLAGS(1), 0x01ccffff);
    WRITE_REG(MAC_KEY_MATERIAL(1, 0), 0xffffffff);
    WRITE_REG_RMW(MAC_KEY_MATERIAL(1, 1), 0xffff0000, 0x0000ffff);
    WRITE_REG_MASK(MAC_KEY_ENABLE, 1 << 1);
}

// mac init: interrupts off and cleared, crypto off, placeholder keys, the rx dma ring, the rate map and rx filter, then the interrupt set we service; tx stays disabled until the rf is up
void init_wifi_mac(void) {
    WRITE_REG(MAC_INT_ENABLE, 0);
    WRITE_REG(MAC_INT_CLEAR, 0xffffffff);
    mac_options_init();
    WRITE_REG(MAC_CRYPTO_CIPHER, MAC_CRYPTO_OFF);
    WRITE_REG(MAC_CRYPTO_CONF, MAC_CRYPTO_OFF);
    key_table_init();
    // third crypto word cleared; its meaning is not known
    WRITE_REG(0x3ff20808, 0);
    init_wifi_dma();
    // rate index map, one nibble per rate (channel 14 remaps every entry to one cck rate), and two all-ones words behind it
    WRITE_REG(0x3ff20400, 0x76503210);
    WRITE_REG(0x3ff20404, 0xbbbbbbbb);
    WRITE_REG(0x3ff20408, 0xbbbbbbbb);
    // address filtering on for all three units and the two upper option bits, then the scan-hold and bit 12 cleared, so nothing is locked to a bssid yet
    WRITE_REG_MASK(MAC_RX_FILTER, 0x707);
    WRITE_REG_UNMASK(MAC_RX_FILTER, 0x00000010);
    WRITE_REG_UNMASK(MAC_RX_FILTER, 0x00001000);
    // both bssid units disabled until the station picks an ap
    WRITE_REG_UNMASK(MAC_BSSID_MASK_HI(0), MAC_ADDR_MATCH_ENABLE);
    WRITE_REG_UNMASK(MAC_BSSID_MASK_HI(1), MAC_ADDR_MATCH_ENABLE);
    WRITE_REG(MAC_INT_ENABLE, WDEV_INTEREST_EVENT);
    // set after every mac reset before tx is enabled; the bit is not decoded
    WRITE_REG_MASK(0x3ff20178, 2);
    WRITE_REG_UNMASK(MAC_TX_CTRL, MAC_TX_ENABLE);
}

// every address byte masked on unit 0 and the promiscuous filter bits set, so the scan hears every frame on the channel
static void mac_filter_accept_all(void) {
    WRITE_REG(MAC_ADDR_LO(0), 0xffffffff);
    WRITE_REG(MAC_ADDR_HI(0), 0x0000ffff);
    WRITE_REG(MAC_ADDR_MASK_HI(0), 0x0001ffff);
    WRITE_REG(MAC_ADDR_MASK_LO(0), 0xffffffff);
    WRITE_REG(MAC_BSSID_LO(0), 0xffffffff);
    WRITE_REG(MAC_BSSID_HI(0), 0x0000ffff);
    WRITE_REG(MAC_BSSID_MASK_LO(0), 0xffffffff);
    WRITE_REG_MASK(MAC_RX_FILTER, 1);
    WRITE_REG_MASK(MAC_RX_FILTER, 2);
}

void wifi_mac_rx_enable(void) {
    rf_off();
    wait_us(2000);
    wifi_rf_on();

    // this is the sniffer entry sequence: address filters off, so every frame on the channel reaches the scan
    mac_filter_accept_all();
    WRITE_REG_UNMASK(MAC_RX_FILTER, 0x00000001);
    WRITE_REG_UNMASK(MAC_RX_FILTER, 0x00000002);
    WRITE_REG_UNMASK(MAC_RX_FILTER, 0x00000004);

    // raw delivery of protected frames, crypto pass-through on and the engine off, so encrypted beacons and data are still handed up
    WRITE_REG_MASK(MAC_RX_OPTION, 0x00040000);
    WRITE_REG_MASK(MAC_CRYPTO_CIPHER, 0x03000000);
    WRITE_REG_UNMASK(MAC_CRYPTO_CIPHER, 0x00010000);
    WRITE_REG_MASK(MAC_CRYPTO_CONF, 0x03000000);
    WRITE_REG_UNMASK(MAC_CRYPTO_CONF, 0x00010000);

    // unit 0 masks no address bytes but stays enabled, so it matches everything
    WRITE_REG(MAC_ADDR_MASK_LO(0), 0);
    WRITE_REG(MAC_ADDR_MASK_HI(0), MAC_ADDR_MATCH_ENABLE);
    WRITE_REG(MAC_BSSID_MASK_LO(0), 0);
    WRITE_REG(MAC_BSSID_MASK_HI(0), MAC_ADDR_MATCH_ENABLE);

    // the two sniffer rx events added to the serviced set, so unfiltered frames raise the fiq
    WRITE_REG(MAC_INT_ENABLE, WDEV_INTEREST_EVENT | WDEV_SNIFFER_EVENT);

    // baseband rx mode bits that the sniffer runs without; the delay lets the rx path settle before the mac transmit enable is dropped, which stops acks
    WRITE_REG_UNMASK(0x60009d44, 0x24000000);
    wait_us(15000);
    WRITE_REG_UNMASK(MAC_TX_OPTION, 0x00000001);

    WRITE_REG_MASK(MAC_TX_CTRL, MAC_TX_ENABLE);

    // tx i/q calibration registers to fixed values, so no i/q measurement runs before transmitting
    WRITE_REG(0x6000983c, 0x00000012);
    WRITE_REG(0x60009860, 0x02230001);
    WRITE_REG_UNMASK(0x60009864, 0x00000100);
    // spur protection, rx debug and other baseband words; these are the known-working values, none of them has been tuned here
    WRITE_REG(0x60009884, 0x00018000);
    WRITE_REG(0x6000989c, 0x00018000);
    WRITE_REG(0x600098a0, 0xf1ac6667);
    WRITE_REG(0x600098d4, 0x00000001);
    WRITE_REG(0x600099ac, 0x00000005);
    WRITE_REG(0x600099b0, 0x00000001);
    WRITE_REG_MASK(BB_TX_CAL, 0x00000020);
    WRITE_REG(0x60009ab4, 0x00e6fffd);
    WRITE_REG(0x60009ab8, 0x00000006);
    WRITE_REG(0x60009abc, 0x01800001);
    WRITE_REG(0x60009b4c, 0x013c0000);
    WRITE_REG(0x60009c44, 0x00000000);
    WRITE_REG(0x60009d0c, 0x0000000a);

    // one tx gain and attenuation word for every rate slot, so all rates transmit at the same level
    for (unsigned int i = 0; i < RF_TXPWR_REGS; i++)
        WRITE_REG(RF_TXPWR_REG(i), 0x0003e4f3);
    // the noise-floor start word and its neighbour, then one rfpll control bit
    WRITE_REG(0x600005b0, 0x043a0000);
    WRITE_REG(0x600005b8, 0x00034008);
    WRITE_REG_MASK(RFPLL_CTRL, 0x00000100);
    WRITE_REG_UNMASK(0x60000d20, 0x02000000);
    WRITE_REG(0x600005fc, 0x000c0b0a);
    rtc.analog_0 = 0x00200000;
    WRITE_REG_UNMASK(BBPLL_CTRL, 0x10000000);
    // sar adc control (bit 0 is the tx power-detector enable, bit 31 undecoded) and mode registers, its done bits, then its data words; the tx power measurement runs on it
    WRITE_REG_MASK(0x60000d50, 0x80000000);
    WRITE_REG_MASK(0x60000d5c, 0x80000000);
    WRITE_REG(0x60000d60, 0x00000003);
    WRITE_REG(0x60000d80, 0x00000661);
    WRITE_REG(0x60000d84, 0x000005c5);
    WRITE_REG(0x60000d88, 0x000005c5);
    WRITE_REG(0x60000d8c, 0x000005c5);
    WRITE_REG(0x60000d90, 0x00000435);
    WRITE_REG(0x60000d94, 0x00000435);
    WRITE_REG(0x60000d98, 0x00000661);
    WRITE_REG(0x60000d9c, 0x00000663);
}
