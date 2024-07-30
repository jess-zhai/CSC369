#include "sim.h"
#include "coremap.h"

int clock_hand;
extern struct frame* coremap;
/* Page to evict is chosen using the CLOCK algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int clock_evict(void)
{
    while (1) {
        if (!get_referenced(coremap[clock_hand].pte)) {
            int victim = clock_hand;
            clock_hand = (clock_hand + 1) % memsize; // update clock hand even if found victim
            return victim;
        } else {
            set_referenced(coremap[clock_hand].pte, false);
            clock_hand = (clock_hand + 1) % memsize;
        }
    }
    
}

/* This function is called on each access to a page to update any information
 * needed by the CLOCK algorithm.
 * Input: The page table entry and full virtual address (not just VPN)
 * for the page that is being accessed.
 */
void clock_ref(int frame, vaddr_t vaddr)
{
    set_referenced(coremap[frame].pte, true);
    (void)vaddr;
}

/* Initialize any data structures needed for this replacement algorithm. */
void clock_init(void)
{
    clock_hand = 0;
}

/* Cleanup any data structures created in clock_init(). */
void clock_cleanup(void)
{

}
