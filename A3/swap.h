#ifndef __SWAP_H__
#define __SWAP_H__

#include <sys/types.h>

#define INVALID_SWAP (off_t)-1

// Swap functions for use in other files
extern void swap_init(size_t size);
extern void swap_destroy(bool free_bitmap);

// Read data into (simulated) physical memory 'frame' from 'offset'
// in swap file.
// Input:  frame - the physical frame number (not byte offset) in physmem
//         offset - the byte position in the swap file
// Return: 0 on success,
//         -errno on error or number of bytes read on partial read
extern int swap_pagein(unsigned int frame, off_t offset);

// Write data from (simulated) physical memory 'frame' to 'offset'
// in swap file. Allocates space in swap file for virtual page if needed.
// Input:  frame - the physical frame number (not byte offset in physmem)
//         offset - the byte position in the swap file
// Return: the offset where the data was written on success,
//         or INVALID_SWAP on failure
extern off_t swap_pageout(unsigned int frame, off_t offset);


#endif /* __SWAP_H__ */
