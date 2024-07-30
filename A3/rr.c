#include "sim.h"
#include "coremap.h"


/* Page to evict is chosen using the Round Robin algorithm.
 * Since our simulated traces have only one process that never frees
 * memory regions, this is equivalent to FIFO.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int rr_evict(void)
{
	static int i = 0;
	int victim = i;
	i = (i + 1) % memsize;
	return victim;
}

/* This function is called on each access to a page to update any information
 * needed by the Round Robin algorithm.
 * Input: The page table entry and full virtual address (not just VPN)
 * for the page that is being accessed.
 */
void rr_ref(int frame, vaddr_t vaddr)
{
	(void)frame;
	(void)vaddr;
}

/* Initialize any data structures needed for this replacement algorithm. */
void rr_init(void)
{
}

/* Cleanup any data structures created in rr_init(). */
void rr_cleanup(void)
{
}
