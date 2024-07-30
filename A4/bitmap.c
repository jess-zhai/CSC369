/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: OS/161 developers, Angela Demke Brown
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2022 Angela Demke Brown
 */

/**
 * CSC369 Assignment 4 - bitmap utility functions.
 */

#include "bitmap.h"

// The bitmap code is modified from the OS/161 bitmap functions,
// and the A3 swap.c code. The main distinction from A3 is that
// the space for the filesystem bitmaps is determined by the
// filesystem layout, not dynamically allocated by malloc.
// Also, the bitmap structure does not include the size.

static const size_t bits_per_word = sizeof(size_t) * CHAR_BIT;
static const size_t word_all_bits = (size_t)-1;

// Initialize the first nbits bits of bitmap to 0 (meaning available).
int bitmap_init(bitmap_t *b, uint32_t nbits)
{
	uint32_t nwords = div_round_up(nbits, bits_per_word);
	assert(b != NULL);
	size_t *words = (size_t *)b;

	memset(words, 0, nwords * sizeof(size_t));

	// Mark any leftover bits at the end of last word as not available
	if (nwords > nbits / bits_per_word) {
		uint32_t idx = nwords - 1;
		uint32_t overbits = nbits - idx * bits_per_word;

		assert(nbits / bits_per_word == nwords - 1);
		assert(overbits > 0 && overbits < bits_per_word);

		for (uint32_t j = overbits; j < bits_per_word; ++j) {
			words[idx] |= ((size_t)1 << j);
		}
	}

	return 0;
}

// Find the first unused bit in bitmap b and return the index of the bit in *index.
// Returns 0 on success and -1 if all bits are already marked as in-use.
int bitmap_alloc(bitmap_t *b, uint32_t nbits, uint32_t *index)
{
	uint32_t max_idx = div_round_up(nbits, bits_per_word);
	size_t *words = (size_t *)b;

	for (uint32_t idx = 0; idx < max_idx; ++idx) {
		if (words[idx] != word_all_bits) {
			for (uint32_t offset = 0; offset < bits_per_word; ++offset) {
				size_t mask = (size_t)1 << offset;

				if ((words[idx] & mask) == 0) {
					words[idx] |= mask;
					*index = (idx * bits_per_word) + offset;
					assert(*index < nbits);
					return 0;
				}
			}
			assert(false);
		}
	}
	return -1;
}

// Marks the bit at the given index as available (0).
// The supplied index must be less than the number of bits in the bitmap.
// The bitmap at the supplied index must be marked allocated.
void bitmap_free(bitmap_t *b, uint32_t nbits, uint32_t index)
{
	uint32_t idx = index / bits_per_word;
	uint32_t offset = index % bits_per_word;
	size_t mask = (size_t)1 << offset;
	size_t *words = (size_t *)b;

	assert(index < nbits);
	assert((words[idx] & mask) != 0); // Don't free something not allocated.

	words[idx] &= ~mask;
}



// Set the bit at index to 0 if val==false, or 1 if val == true
void bitmap_set(bitmap_t *b, uint32_t nbits, uint32_t index, bool val)
{
	assert(index < nbits);
	uint32_t idx = index / bits_per_word;
	size_t mask = (size_t)1 << (index % bits_per_word);
	size_t *words = (size_t *)b;

	if (val == true) {
		// Set bit at index to 1
		words[idx] |= mask;
	} else {
		// Set bit at index to 0
		words[idx] &= ~mask;
	}	
}

// Returns true is the bit at index is set to 1, otherwise false
bool bitmap_isset(bitmap_t *b, uint32_t nbits, uint32_t index)
{
	assert(index < nbits);
	uint32_t idx = index / bits_per_word;
	size_t mask = (size_t)1 << (index % bits_per_word);
	size_t *words = (size_t *)b;
	return ((words[idx] & mask) > 0);
}

