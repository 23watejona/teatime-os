#include "uart.h"
#include "mem.h"


extern int _heap_start;
extern int _heap_end;


int *heap_end = &_heap_end;
int *heap_start = &_heap_start;

struct memblk_t *freelist;

int initmem() {
	
	// round down to block size
	unsigned int heap_size = (unsigned int)heap_end - (unsigned int)heap_start;
	heap_size = floor_memblk(heap_size);
	heap_end = (int *)((unsigned int) heap_start + heap_size);
	
	kprintf_uart("Heap start: %x\n", heap_start);
	kprintf_uart("Heap end: %x\n", heap_end);
	kprintf_uart("Heap size: %u\n", heap_size);

	freelist = (struct memblk_t *)((unsigned int)heap_start);
	freelist->next = NULL;
	freelist->size = heap_size;
	return 0;
}
