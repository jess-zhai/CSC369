#include "sim.h"
#include "coremap.h"
#include <string.h>

/*
 * Allocates a frame to be used for the virtual page represented by pte.
 * If all frames are in use, calls the replacement algorithm's evict_func to
 * select a victim frame. Writes victim to swap if needed, and updates
 * page table entry for victim to indicate that virtual page is no longer in
 * (simulated) physical memory.
 */
int allocate_frame(struct pt_entry_s *pte)
{
	int frame = -1;
	for (size_t i = 0; i < memsize; ++i) {
		if (!coremap[i].in_use) {
			frame = i;
			break;
		}
	}

	if (frame == -1) { // Didn't find a free page.
		// Call replacement algorithm's evict function to select victim
		frame = evict_func();
		assert(frame != -1);

		// All frames were in use, so victim frame must hold some page
		// Write victim page to swap, if needed, and update page table
		struct pt_entry_s *victim = coremap[frame].pte;
		assert(victim != NULL);
		handle_evict(victim);
	}

	// Record information for virtual page that will now be stored in frame
	coremap[frame].in_use = true;
	coremap[frame].pte = pte;

	return frame;
}

/*
 * Initializes the content of a (simulated) physical memory frame when it
 * is first allocated for some virtual address. Just like in a real OS, we
 * fill the frame with zeros to prevent leaking information across pages.
 */
void init_frame(int frame)
{
	// Calculate pointer to start of frame in (simulated) physical memory
	unsigned char *mem_ptr = &physmem[frame * SIMPAGESIZE];
	memset(mem_ptr, 0, SIMPAGESIZE); // zero-fill the frame
}


/*
 * Return the physical memory address corresponding to the virtual
 * address.
 */
unsigned char *find_physpage(vaddr_t vaddr, char type)
{
	int frame = find_frame_number(vaddr, type);

	// Call replacement algorithm's ref_func for this page.
	assert(frame != -1);
	ref_func(frame, vaddr);

	// Return pointer into (simulated) physical memory at start of frame
	return &physmem[frame * SIMPAGESIZE];
}
