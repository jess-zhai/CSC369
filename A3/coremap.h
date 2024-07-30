#ifndef __COREMAP_H__
#define __COREMAP_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <assert.h>
#include "list.h"

// This file contains definitions that are needed to manage physical frames
// of memory. Everything in this file should be independent of the page table
// format.
// All definitions that are specific to a particular page table format or
// implementation should go in pagetable.h

// User-level virtual addresses on a 64-bit Linux system are 48 bits in our
// traces, and the page size is 4096 bytes (log2(4096) = 12 bits for offset). 
#define PAGE_SIZE  4096
#define PAGE_SHIFT 12
#define PAGE_MASK  (~(PAGE_SIZE - 1))

// Page table entry - actual definition will go in pagetable.h or pagetable.c
struct pt_entry_s; 

/* The coremap holds information about physical memory.
 * The index into coremap is the physical page frame number stored
 * in the page table entry (pt_entry_t).
 */
struct frame {
	bool in_use;    // true if frame is allocated, false if frame is free
	struct pt_entry_s *pte; // Pointer back to pagetable entry (pte) for page
	                        // stored in this frame
	list_entry framelist_entry;
};

extern struct frame *coremap;

// Coremap functions that your pagetable should call.
int allocate_frame(struct pt_entry_s * pte);
void init_frame(int frame);

// Accessor functions for coremap, for pagetable specific handling
// logic that you need to implement
void handle_evict(struct pt_entry_s * pte);
int find_frame_number(vaddr_t vaddr, char type);

// Accessor functions for page table entries, to allow replacement
// algorithms to obtain information from a PTE, without depending
// on the internal implementation of the structure.
bool is_valid(struct pt_entry_s *pte);
bool is_dirty(struct pt_entry_s *pte);
bool get_referenced(struct pt_entry_s *pte);
void set_referenced(struct pt_entry_s *pte, bool val);

// The replacement algorithms.
#define REPLACEMENT_ALGORITHMS \
	RA(rand) \
	RA(rr) \
	RA(clock) \
	RA(s2q) 
// no longer part of the assignment: lru, mru, opt

// Replacement algorithm functions.
// These may not need to do anything for some algorithms.
#define RA(name) \
	void name ## _init(void); \
	void name ## _cleanup(void); \
	void name ## _ref(int frame, vaddr_t vaddr); \
	int name ## _evict(void);
REPLACEMENT_ALGORITHMS
#undef RA

#endif /* __COREMAP_H__ */
