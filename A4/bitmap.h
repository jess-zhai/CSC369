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

#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

#include "util.h"

// The bitmap code is modified from the OS/161 bitmap functions,
// and the A3 swap.c code. The main distinction from A3 is that
// the space for the filesystem bitmaps is determined by the
// filesystem layout, not dynamically allocated by malloc.


typedef size_t bitmap_t;

// Initialize the first nbits bits of bitmap to 0 (meaning available).
int bitmap_init(bitmap_t *b, uint32_t nbits);

// Find the first unused bit in bitmap b and return the index of the bit in *index.
// Returns 0 on success and -1 if all bits are already marked as in-use.
int bitmap_alloc(bitmap_t *b, uint32_t nbits, uint32_t *index);

// Marks the bit at the given index as available (0).
// The supplied index must be less than the number of bits in the bitmap.
// The bitmap at the supplied index must be marked allocated.
void bitmap_free(bitmap_t *b, uint32_t nbits, uint32_t index);

// Set the bit at index to 0 if val==false, or 1 if val == true
void bitmap_set(bitmap_t *b, uint32_t nbits, uint32_t index, bool val);

// Returns true is the bit at index is set to 1, otherwise false
bool bitmap_isset(bitmap_t *b, uint32_t nbits, uint32_t index);
