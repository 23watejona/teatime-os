#include "uart.h"
#include "reg_util.h"
#include "proc.h"
#include "proc_queue.h"

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
void init_wifi_mac_addr(void);
void wifi_set_channel(unsigned int);
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

    
    init_wifi_clk();
    init_wifi_pbus();
    init_wifi_iomux();
    init_wifi_bb();
    init_wifi_dma();
    init_wifi_mac();
    init_wifi_mac_addr();
    wifi_set_channel(1);

    int pid = create(main, INIT_STK, 5);
    kprintf_uart("created main as pid %d\n", pid);
    make_avail(pid);

    wifi_rx_servicer_pid = create(wifi_rx_servicer, INIT_STK, 10);
    make_avail(wifi_rx_servicer_pid);

    // enable timer + WiFi MAC (INUM 0) interrupts
    intr_unmask(1u << 0);
    init_cpu_timer();
    while (1) {
        asm("waiti 0");
    }
}
