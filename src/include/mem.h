#ifndef MEM_H
#define MEM_H
#include "def.h"

#define MIN_ALLOC (16)
#define floor_memblk(size) ((unsigned int)size & ~7u)
#define ceil_memblk(size) ((7 + (unsigned int)size) & ~7u)

extern int *heap_end;
extern int *heap_start;

typedef struct memblk_t {
	unsigned int size;
	union {
		struct memblk_t *next;
		char data[0];
	};
} memblk_t;

#define ALLOC_MEMBLK_SIZE (sizeof(memblk_t) - (sizeof(memblk_t *)))
#define UNALLOC_MEMBLK_SIZE (sizeof(memblk_t))

extern memblk_t *freelist;
#endif
