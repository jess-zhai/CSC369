#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "khash.h"
#include "interrupt.h"

/* Need 2^63 bytes malloced before these will overflow as 
 * signed types, and having signs makes the math safer
 * if the accounting is wrong.
 */
long num_mallocs; /* Total number of malloc369 calls */
long num_frees;   /* Total number of free369 calls */ 
long bytes_malloced; /* Total number of bytes malloced */
long bytes_freed;    /* Total number of bytes freed */

static bool verbose;

/* Add some status bits to the 'size' stored in the malloc map */
#define FREED   0x8000000000000000 /* if set, ptr has been freed already */

KHASH_MAP_INIT_INT64(ptrmap, size_t)
khash_t(ptrmap) *malloc_map;
		
extern void * malloc369(size_t size)
{
	num_mallocs++;
	bytes_malloced += size;

	void * m = malloc(size);
	if (m == NULL) {
		exit(-1);
	}
	//malloc_map[m] = size;
	/* Record the ptr for later free tracking */
	int ret;
	khiter_t k = kh_put(ptrmap, malloc_map, (size_t)m, &ret);
	assert(ret >= 0);
	if (ret == 0 && verbose) { /* key was present and not deleted */
		unintr_printf("malloc369 - malloc returned ptr that we did not delete!\n");
	}
	kh_value(malloc_map, k) = size;
	
	return m;
}

extern void free369(void * ptr)
{
        size_t size = 0;
	bool is_missing = true;
        khiter_t k;
		
	if (ptr == NULL) {
		/* Ok to free(NULL) but we don't want to count that as  
		 * matching an actual malloc.
		 */
		free(ptr);
		return;
	}

	k = kh_get(ptrmap, malloc_map, (size_t)ptr);
	is_missing = (k == kh_end(malloc_map));
	
	/* Get the size and check if we are trying to free an address that 
	 * we didn't get from malloc. 
	 */

	if (!is_missing) {
		size = kh_value(malloc_map, k);
	} else {
		if (verbose) {
			unintr_printf("free369 - trying to free a ptr that is "
				      "not in our map!\n");
		}
		free(ptr); /* Should abort if our map is correct. */
		return;
	}

	
	/* Check for double-free.
	 */
	if (size & FREED) {
		if (verbose) {
			unintr_printf("free of already freed ptr %p detected!\n",
				      ptr);
		}
		free(ptr); /* Should abort if our check is correct */
		return;
	}
	
	/* Count one more free of size bytes */
	assert(size != 0);
	assert(num_mallocs - num_frees > 0);
	num_frees++;
	assert((bytes_malloced - bytes_freed) >= size);
	bytes_freed += size;

	/* Fill freed memory with 0xee to help detect use-after-free bugs. */
	/* Why 0xee? Because (a) filling with 0xff can look like -1 which might
	 * be misleading, and (b) filling with a hex-word like '0xdead' 
	 * requires either an assumption that malloc'd sizes are always even
	 * or more complicated code to check if size is even or odd. 
	 * Depending on how you look at things you may see memory containing
	 * 0xee in different ways. For example, when viewed as:
	 *     char:     0xee (1 byte) = -18
	 *     unsigned char: 0xee (1 byte) = 238      
	 *     Viewed as an int:   0xeeeeeeee (4 bytes) = -286331154
	 *     Viewed as unsigned: 0xeeeeeeee (4 bytes) = 4008636142
	 *     Viewed as long: 0xeeeeeeeeeeeeeeee (8 bytes) = -1229782938247303442
	 *     Viewed as unsigned long: 0xeeeeeeeeeeeeeeee (8 bytes) = 17216961135462248174
	 *     Viewed as ptr: 0xeeeeeeeeeeeeeeee (8 bytes) = 0xeeeeeeeeeeeeeeee
	 *
	 * Looking at memory in hex, or as (void *) type in gdb will make it
	 * easy to spot the 'freed memory chunk' pattern. 
	 */

	char *region = (char *)ptr;
	for (size_t i = 0; i < size; i++) {
		region[i] = 0xee;
	}
	free(ptr);
	kh_value(malloc_map, k) |= FREED;
	
}


extern void init_csc369_malloc(bool verb)
{
        malloc_map = kh_init(ptrmap);
	verbose = verb;
	num_mallocs = 0;
	bytes_malloced = 0;
	num_frees = 0;
	bytes_freed = 0;
}

extern long get_current_bytes_malloced()
{
	assert(bytes_malloced >= bytes_freed);
	return (bytes_malloced - bytes_freed);
}

extern long get_current_num_mallocs()
{
	assert(num_mallocs >= num_frees);
	return (num_mallocs - num_frees);
}

extern long get_num_mallocs()
{
	return num_mallocs;
}

extern long get_bytes_malloced()
{
	return bytes_malloced;
}

/* Pass in 'tolerance' for number of mallocs and bytes malloc'd that we 
 * won't consider a leak.
 */
extern  bool is_leak_free(int num_mallocs_tol, int num_bytes_tol)
{
	if (get_current_bytes_malloced() > num_bytes_tol ||
	    get_current_num_mallocs() > num_mallocs_tol) {
		return false;
	} else {
		return true;
	}
}
