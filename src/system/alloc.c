#include "uart.h"
#include "mem.h"

char *alloc(unsigned int size) {
	size = ceil_memblk(size + ALLOC_MEMBLK_SIZE);
	if (size < MIN_ALLOC) {
		size = MIN_ALLOC;
	}
	memblk_t *prev = NULL;
	struct memblk_t *itr = freelist;
	while (itr != NULL) {
		if (itr->size > size) {
			if (prev) {
				prev->next = itr->next;
			} else {
				freelist = itr->next;
			}
			// can't split
			if (itr->size < size + MIN_ALLOC) {
				return itr->data;
			}
			// split the block
			memblk_t *ret = itr; // returned block
			memblk_t *rem = (memblk_t *)((char *)itr + size); // remaining block
			rem->size = itr->size - size;
			ret->size = size;

			if (prev) {
				rem->next = prev->next;
				prev->next = rem;
			} else {
				rem->next = freelist;
				freelist = rem;
			}

			return ret->data;
		}
		prev = itr;
		itr = itr->next;
	}
	return NULL;
}


char *alloc_stack(unsigned int size) {
	size = ceil_memblk(size + ALLOC_MEMBLK_SIZE);
	if (size < MIN_ALLOC) {
		size = MIN_ALLOC;
	}
	memblk_t *prev = NULL;
	struct memblk_t *itr = freelist;
	memblk_t *poten_prev = NULL;
	memblk_t *poten = NULL;
	while (itr != NULL) {
		if (itr->size > size) {
			poten_prev = prev;
			poten = itr;
		}
		prev = itr;
		itr = itr->next;
	}
	
	if (!poten) {
		return NULL;
	}
	
	if (poten_prev) {
		poten_prev->next = poten->next;
	} else {
		freelist = poten->next;
	}

	// can't split
	if (poten->size < size + MIN_ALLOC) {
		return poten->data;
	}

	// split the block
	memblk_t *rem = poten; // returned block
	rem->size = poten->size - size;

	memblk_t *ret = (memblk_t *)((char *)poten + poten->size); // remaining block
	ret->size = size;

	if (poten_prev) {
		rem->next = poten_prev->next;
		poten_prev->next = rem;
	} else {
		rem->next = freelist;
		freelist = rem;
	}

	return ret->data;
}
