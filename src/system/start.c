#include "def.h"
#include "uart.h"
#include "reg_util.h"
#include "proc.h"
#include "proc_queue.h"
#include "wait.h"
#include "wdt.h"

#define NULL_STK 1024
#define INIT_STK 2048
#define SERVICER_STK 4096 /* WPA HMAC peak + a tick's frames measured ~2112 */


void ctxsw(unsigned int **, unsigned int **);
int initmem(void);
char *alloc_stack(unsigned int);
void _set_vec_base(void);
int init_cpu_clk(unsigned int clk_rate_mhz);
void init_cpu_timer(void);
void init_wifi_clk(void);
void init_wifi_pbus(void);
void init_wifi_iomux(void);
void init_wifi_bb(void);
void init_wifi_dma(void);
void init_wifi_mac(void);
void wifi_mac_rx_enable(void);
void init_wifi_mac_addr(void);
void init_wifi_rf(void);
void wifi_secrets_init(void);
void wifi_set_channel(int);
void startup_proc(void);
void wifi_rx_servicer(void);
void main(void);
int disable();

extern int wifi_rx_servicer_pid;

unsigned int intr_unmask(unsigned int);

extern queue_entry *avail_list;
extern unsigned int _bss_start, _bss_end;
extern unsigned int _nmi_stack_bottom;

proctab_entry proctab[NUM_PROC] = {0};
int curr_pid = 0;
unsigned int boot_reset_cause;

#define CACHE_CTRL  0x3ff0000c
#define CACHE_CTRL2 0x3ff00024
#define SPI0_CTRL   0x60000208

/* Map the first MB of flash at 0x40200000. Runs with the cache off, so it and
   everything it touches must live in IRAM. */
IRAM_ATTR static void flash_cache_enable(void)
{
    while (READ_REG(CACHE_CTRL) & 0x100)
        WRITE_REG_UNMASK(CACHE_CTRL, 0x100); // must be off before the flush below
    asm volatile("memw");

    WRITE_REG_UNMASK(SPI0_CTRL, 0x20000); // spi0 cache arbitration, held off across the flush
    asm volatile("memw");

    WRITE_REG_UNMASK(CACHE_CTRL, 1);
    WRITE_REG_MASK(CACHE_CTRL, 1);
    while (!(READ_REG(CACHE_CTRL) & 2))
        ;
    WRITE_REG_UNMASK(CACHE_CTRL, 1);
    asm volatile("memw");

    WRITE_REG_MASK(SPI0_CTRL, 0x20000);
    asm volatile("memw");

    // block select and count cleared, single-mb mode: maps the first mb of flash
    WRITE_REG_UNMASK(CACHE_CTRL, 0x03000000);
    WRITE_REG_RMW(CACHE_CTRL, 0xfbf8ffff, 1u << 26);
    WRITE_REG_MASK(CACHE_CTRL2, 0x18);
    asm volatile("memw");

    WRITE_REG_MASK(CACHE_CTRL, 0x100);
    while (!(READ_REG(CACHE_CTRL) & 0x100))
        ;
}

IRAM_ATTR void start ( void )
{
    for (unsigned int *p = &_bss_start; p < &_bss_end; ++p)
        *p = 0;

    // everything but start.o and the interrupt path runs from flash, so this must be the first call
    flash_cache_enable();

    _set_vec_base();
    BUSY_WAIT();
    init_cpu_clk(80);
    initmem();
    boot_reset_cause = READ_REG(0x60000714) & 0xf; // 4 is the hardware wdt; read after the bss clear above, which would wipe it
    
    alloc_stack(NULL_STK);

    for (int i = 0; i < NUM_PROC; ++i) {
        proctab[i].status = PROC_UNUSED;
    }

    proctab_entry *null_proc_entry = &proctab[0];
    null_proc_entry->status = PROC_CURR;
    null_proc_entry->priority = PROC_NULL_PRIO;
    curr_pid = 0;
    avail_list = new_queue();
    sem_init();
    io_wait_init();
    make_avail(NULL_PROC);

    
    // boot safe window: WDT-protected reflash interval before risky RF init
    wdt_enable();
    kprintf_uart("\n=== SAFE WINDOW 5s: flash/recover now ===\n");
    for (int s = 5; s > 0; --s) {
        kprintf_uart("%d ", s);
        wdt_feed();
        wait_us(1000000);
    }
    kprintf_uart("\nproceeding\n");
    // the breadcrumbs live in rtc ram and survive wdt resets, so zero them after printing
    kprintf_uart("boot: reset cause %d nmi_epc3=%x dexc_epc1=%x dexc_cause=%x "
                 "dexc_depc=%x dexc_vaddr=%x fatal_epc1=%x fatal_cause=%x "
                 "fatal_a0=%x fatal_sp=%x\n",
                 boot_reset_cause, READ_REG(0x60001200), READ_REG(0x60001204),
                 READ_REG(0x60001208), READ_REG(0x6000120c),
                 READ_REG(0x60001210), READ_REG(0x60001214),
                 READ_REG(0x60001218), READ_REG(0x6000121c),
                 READ_REG(0x60001220));
    WRITE_REG(0x60001200, 0);
    WRITE_REG(0x60001204, 0);
    WRITE_REG(0x60001208, 0);
    WRITE_REG(0x6000120c, 0);
    WRITE_REG(0x60001210, 0);
    WRITE_REG(0x60001214, 0);
    WRITE_REG(0x60001218, 0);
    WRITE_REG(0x6000121c, 0);
    WRITE_REG(0x60001220, 0);

    wdt_feed();
    wifi_secrets_init();
    kprintf_uart("wifi: clk\n");
    init_wifi_clk();
    kprintf_uart("wifi: pbus\n");
    init_wifi_pbus();
    kprintf_uart("wifi: iomux\n");
    init_wifi_iomux();
    wdt_feed();
    kprintf_uart("wifi: rf\n");
    init_wifi_rf();
    kprintf_uart("wifi: mac\n");
    init_wifi_mac();
    kprintf_uart("wifi: mac_addr\n");
    init_wifi_mac_addr();
    _nmi_stack_bottom = STK_CANARY(NUM_PROC);
    // arm the wifi nmi gate; from here on only the tick pulses it
    WRITE_REG(0x3ff00000, (READ_REG(0x3ff00000) & 0xffffffe0) | 1);
    kprintf_uart("wifi: rx_enable\n");
    wifi_mac_rx_enable();
    kprintf_uart("wifi: done\n");
    wdt_feed();

    int pid = create(main, INIT_STK, 5);
    kprintf_uart("created main as pid %d\n", pid);
    make_avail(pid);

    wifi_rx_servicer_pid = create(wifi_rx_servicer, SERVICER_STK, 10);
    make_avail(wifi_rx_servicer_pid);

    // enable timer interrupt (WiFi RX is serviced via the NMI/FIQ path)
    init_cpu_timer();
    while (1) {
        asm("waiti 0");
    }
}
