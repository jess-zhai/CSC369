#ifndef __SIM_H__
#define __SIM_H__

#include "timer.h"

typedef unsigned long vaddr_t; /* virtual address is 48 bits, need long type */

#define SIMPAGESIZE 16         /* Simulated physical memory page frame size */
extern unsigned char *physmem; /* Array of bytes to simulate physical memory */
extern size_t memsize;         /* Number of frames of physical memory */
extern int debug;              /* Control amount of debugging output */


/* Interface to pagetable functions that are called from sim.c */
extern void init_pagetable(void);
extern void print_pagetable(void);
extern void free_pagetable(void);
extern unsigned char *find_physpage(vaddr_t vaddr, char type);


/* Counters for paging-related events. Set in pagetable.c, reported by sim.c */
extern size_t hit_count;
extern size_t miss_count;
extern size_t ref_count;
extern size_t evict_clean_count;
extern size_t evict_dirty_count;

/* Pointers to per-eviction algorithm functions needed in pagetable.c */
extern void (*ref_func)(int frame, vaddr_t vaddr);
extern int (*evict_func)(void);

#endif /* __SIM_H__ */
