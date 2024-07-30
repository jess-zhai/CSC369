/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Alexey Khrabrov, Karen Reid, Angela Demke Brown
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2022 Angela Demke Brown
 */

/**
 * CSC369 Assignment 4 - File system runtime context header file.
 */

#pragma once

//#include <stdlib.h>
#include <stddef.h>
//#include <unistd.h>
//#include <sys/types.h>
#include "options.h"
#include "vsfs.h"
#include "bitmap.h"

/**
 * Mounted file system runtime state - "fs context".
 */
typedef struct fs_ctx {
	/** Pointer to the start of the image. */
	void *image;
	/** Image size in bytes. */
	size_t size;
	/** Pointer to the superblock in the mmap'd disk image */
	vsfs_superblock *sb;
	/** Pointer to the inode bitmap in the mmap'd disk image */
	bitmap_t *ibmap;
	/** Pointer to the data block bitmap in the mmap'd disk image */
	bitmap_t *dbmap;
	/** Pointer to the inode table in the mmap'd disk image */
	vsfs_inode *itable;
	
	//TODO: other useful runtime state of the mounted file system should be
	//       cached here (NOT in global variables in vsfs.c)

} fs_ctx;

/**
 * Initialize file system context.
 *
 * @param fs     pointer to the context to initialize.
 * @param size   image size in bytes.
 * @return       true on success; false on failure (e.g. invalid superblock).
 */
bool fs_ctx_init(fs_ctx *fs, void *image, size_t size);

/**
 * Destroy file system context.
 * Must cleanup all the resources created in fs_ctx_init().
 * 
 * @param fs     pointer to the context to clean up
 */
void fs_ctx_destroy(fs_ctx *fs);
