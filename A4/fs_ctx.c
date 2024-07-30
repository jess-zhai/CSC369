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
 * CSC369 Assignment 4 - File system runtime context implementation.
 */

#include "fs_ctx.h"

/**
 * Initialize file system context.
 * 
 * @param fs     pointer to the context to initialize.
 * @param image  pointer to the start of the image.
 * @param size   image size in bytes.
 * @return       true on success; false on failure (e.g. invalid superblock).
 */
bool fs_ctx_init(fs_ctx *fs, void *image, size_t size)
{
	// Check if the file system image can be mounted and initialize its
	// runtime state.

	fs->image = image;
	fs->size = size;

	/** VSFS Superblock is first block on disk, so the pointer to the 
	 *  superblock is the same as the pointer to the start of the 
	 *  mmap'd disk image.
	 */
	fs->sb = (vsfs_superblock *)image;

	/** We're very trusting. If the magic number looks good, we'll go 
	 *  ahead and mount the file system (and try to use it).
	 *  You may want to add more sanity checking to make sure the disk
	 *  image appears to be a valid VSFS file system.
	 */
	if (fs->sb->sb_magic != VSFS_MAGIC) {
		return false;
	}
	
	/** VSFS Inode bitmap pointer 
	 *  The block number of the inode bitmap is VSFS_IMAP_BLKNUM; 
	 *  we multiply by the block size to get the offset in bytes from the 
         *  start of the mmap'd disk image.
	 */ 
	fs->ibmap = (bitmap_t *)(image + VSFS_IMAP_BLKNUM * VSFS_BLOCK_SIZE);

	/** VSFS Data block bitmap pointer
	 *  Similar calculation as inode bitmap.
	 */
	fs->dbmap = (bitmap_t *)(image + VSFS_DMAP_BLKNUM * VSFS_BLOCK_SIZE);

	/** VSFS Inode table pointer
	 *  Similar calculation as for bitmaps.
	 */
	fs->itable = (vsfs_inode *)(image + VSFS_ITBL_BLKNUM * VSFS_BLOCK_SIZE);

	// TODO: Initialize anything else that you add to the fs context.
	
	return true;
}


/**
 * Destroy file system context.
 * Must cleanup all the resources created in fs_ctx_init().
 * 
 * @param fs     pointer to the context to clean up
 */
void fs_ctx_destroy(fs_ctx *fs)
{
	//TODO: cleanup any other resources allocated in fs_ctx_init()
	(void)fs;
}
