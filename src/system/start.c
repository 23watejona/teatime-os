#include "uart.h"
#include "reg_util.h"
#include "proc.h"
#include "proc_queue.h"
#include "wait.h"
#include "wdt.h"

#define NULL_STK 1024
#define INIT_STK 2048


void ctxsw(unsigned int **, unsigned int **);
int initmem(void);
char *alloc_stack(unsigned int);
void _set_vec_base(void);
void sched(void);
void make_avail(int);
void unused();
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
void wifi_set_channel(int);
void startup_proc(void);
void wifi_rx_servicer(void);
void main(void);
int disable();

extern int wifi_rx_servicer_pid;

unsigned int intr_unmask(unsigned int);

extern queue_entry *avail_list;
extern unsigned int _bss_start, _bss_end;

proctab_entry proctab[NUM_PROC] = {0};
int curr_pid = 0;

void start ( void )
{
    for (unsigned int *p = &_bss_start; p < &_bss_end; ++p)
        *p = 0;

    _set_vec_base();
    BUSY_WAIT();
    init_cpu_clk(80);
    initmem();
    
    alloc_stack(NULL_STK);

    for (int i = 0; i < NUM_PROC; ++i) {
        proctab[i].status = PROC_UNUSED;
    }

    proctab_entry *null_proc_entry = &proctab[0];
    null_proc_entry->status = PROC_CURR;
    null_proc_entry->priority = PROC_NULL_PRIO;
    curr_pid = 0;
    avail_list = new_queue();
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

    wdt_feed();
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
    // WiFi NMI source left disarmed: it fires correctly on RX events, but the NMI
    // frame handler (ctxsw.s) faults on entry, so the RX servicer polls instead.
    WRITE_REG(0x3ff00000, READ_REG(0x3ff00000) & 0xffffffe0);
    kprintf_uart("wifi: rx_enable\n");
    wifi_mac_rx_enable();
    kprintf_uart("wifi: done\n");
    wdt_feed();

    int pid = create(main, INIT_STK, 5);
    kprintf_uart("created main as pid %d\n", pid);
    make_avail(pid);

    wifi_rx_servicer_pid = create(wifi_rx_servicer, INIT_STK, 10);
    make_avail(wifi_rx_servicer_pid);

    // enable timer interrupt (WiFi RX is serviced via the NMI/FIQ path)
    init_cpu_timer();
    while (1) {
        asm("waiti 0");
    }
}
