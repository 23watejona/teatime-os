#include "uart.h"
#include "reg_util.h"
#include "wifi_dma.h"

extern volatile unsigned int wifi_nmi_count;

void wifi_set_channel(unsigned int ch);
unsigned int rx_digital_stop(void);
void rx_digital_start(unsigned int a2c_saved);
void bbpll_calibrate(unsigned int slow);

void main ( void )
{
    while (1) {
        kprintf_uart("REGDUMP b00=%x b60=%x a2c=%x b64=%x a34=%x\n",
                     READ_REG(0x60009b00), READ_REG(0x60009b60),
                     READ_REG(0x60009a2c), READ_REG(0x60009b64),
                     READ_REG(0x60009a34));
        kprintf_uart("REGDUMP nf24=%x bb28=%x bb2c=%x b08=%x\n",
                     READ_REG(0x60009824), READ_REG(0x60009828),
                     READ_REG(0x6000982c), READ_REG(0x60009b08));
        kprintf_uart("REGDUMP dma00=%x dma04=%x dma08=%x dma0c=%x dma10=%x rxt7c=%x rxh80=%x\n",
                     READ_REG(0x3ff20000), READ_REG(0x3ff20004),
                     READ_REG(0x3ff20008), READ_REG(0x3ff2000c),
                     READ_REG(0x3ff20010), READ_REG(0x3ff2007c),
                     READ_REG(0x3ff20080));
        kprintf_uart("REGDUMP filt6c=%x m178=%x c3c=%x c44=%x evt20=%x e18=%x\n",
                     READ_REG(0x3ff2006c), READ_REG(0x3ff20178),
                     READ_REG(0x3ff20c3c), READ_REG(0x3ff20c44),
                     READ_REG(0x3ff20c20), READ_REG(0x3ff20c18));
        kprintf_uart("REGDUMP clk18=%x nmienb=%x nmi=%d\n",
                     READ_REG(0x3ff00018), READ_REG(0x3ff00000),
                     wifi_nmi_count);
        for (int i = 0; i < 8; ++i)
            kprintf_uart("%d:o%d/l%d ", i, rx_ring[i].owner, rx_ring[i].length);
        kputc_uart('\n');
        BUSY_WAIT();
    }
}
