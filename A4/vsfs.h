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
 * CSC369 Assignment 4 - vsfs types, constants, and data structures header file.
 * 
 * This is the setup for the vsfs (Very Simple File System) described in OSTEP
 * Chapter 40: File System Implementation. 
 */

#pragma once

#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include <sys/stat.h>


/**
 * vsfs block size in bytes. You are not allowed to change this value.
 *
 * The block size is the unit of space allocation. Each file (and directory)
 * must occupy an integral number of blocks. Each of the file systems metadata
 * partitions, e.g. superblock, inode/block bitmaps, inode table (but not an
 * individual inode) must also occupy an integral number of blocks.
 */
#define VSFS_BLOCK_SIZE 4096
#define VSFS_NUM_DIRECT 5

/** Block number (block pointer) type. */
typedef uint32_t vsfs_blk_t;

/** Inode number type. */
typedef uint32_t vsfs_ino_t;


/** Magic value that can be used to identify an vsfs image. */
#define VSFS_MAGIC 0xC5C369A4C5C369A4ul

/* vsfs has simple layout 
 *   Block 0: superblock
 *   Block 1: inode bitmap
 *   Block 2: data bitmap
 *   Block 3: start of inode table
 *   First data block after inode table
 */

#define VSFS_SB_BLKNUM   0
#define VSFS_IMAP_BLKNUM 1
#define VSFS_DMAP_BLKNUM 2
#define VSFS_ITBL_BLKNUM 3


/** vsfs superblock. */

typedef struct vsfs_superblock {	
	uint64_t   sb_magic;       /* Must match VSFS_MAGIC. */
	uint64_t   sb_size;        /* File system size in bytes. */
	uint32_t   sb_num_inodes;  /* Total number of inodes (set by mkfs) */
	uint32_t   sb_free_inodes; /* Number of available inodes */ 
	vsfs_blk_t sb_num_blocks;  /* File system size in blocks */
	vsfs_blk_t sb_free_blocks; /* Number of available blocks in file sys */
	vsfs_blk_t sb_data_region; /* First block after inode table */ 
} vsfs_superblock;

/* Superblock must fit into a single disk sector */
static_assert(sizeof(vsfs_superblock) <= VSFS_BLOCK_SIZE,
              "superblock is too large");

/** vsfs inode. */
typedef struct vsfs_inode {
	/** File mode. */
	mode_t i_mode;

	/**
	 * Reference count (number of hard links).
	 *
	 * Each file is referenced by its parent directory. Each directory is
	 * referenced by its parent directory, itself (via "."), and each
	 * subdirectory (via ".."). The "parent directory" of the root directory
	 * is the root directory itself.
	 */
	uint32_t i_nlink;

	/** File size in vsfs file system blocks */
	vsfs_blk_t i_blocks;
	
	/** File size in bytes. */
	uint64_t i_size;

	/**
	 * Last modification timestamp.
	 *
	 * Must be updated when the file (or directory) is created, written to,
	 * or its size changes. Use the clock_gettime() function from time.h 
	 * with the CLOCK_REALTIME clock; see "man 3 clock_gettime" for details.
	 */
	struct timespec i_mtime;

	/** Data pointers. */
	vsfs_blk_t i_direct[VSFS_NUM_DIRECT];
	vsfs_blk_t i_indirect;
} vsfs_inode;

/** A single block must fit an integral number of inodes */
static_assert(VSFS_BLOCK_SIZE % sizeof(vsfs_inode) == 0, "invalid inode size");

/**
 *  Since we only have 1 inode bitmap block, there can be at most 
 *  VSFS_BLOCK_SIZE * bits_per_byte inodes in the file system.
 */
#define VSFS_INO_MAX VSFS_BLOCK_SIZE*CHAR_BIT

/** 
 * Define the inode number for the root directory.
 */
#define VSFS_ROOT_INO 0

/** The root inode must be in the first block of the inode table. */
static_assert(VSFS_ROOT_INO < (VSFS_BLOCK_SIZE / sizeof(vsfs_inode)),
	      "invalid root inode number");

/**
 *  Since we only have 1 data bitmap block, there can be at most 
 *  VSFS_BLOCK_SIZE * bits_per_byte blocks in the file system.
 */
#define VSFS_BLK_MAX VSFS_BLOCK_SIZE*CHAR_BIT

/**
 *  Since we have a fixed metadata layout, there must be at least
 *  5 blocks in the file system:
 *  superblock, inode bitmap, data bitmap, inode table, root directory data blk
 */
#define VSFS_BLK_MIN 5

/** 
 * Data block numbers must be > VSFS_ITBL_BLKNUM and < VSFS_BLK_MAX
 * for any VSFS file system, but we define 0 as the expected value to use 
 * for an unassigned data block number in an inode or indirect block. 
 */
#define VSFS_BLK_UNASSIGNED 0

/** Maximum file name (path component) length. Includes the null terminator. */
#define VSFS_NAME_MAX 252

/** Maximum file path length. Includes the null terminator. */
#define VSFS_PATH_MAX _POSIX_PATH_MAX

/** Fixed size directory entry structure. */
typedef struct vsfs_dentry {
	/** Inode number. */
	vsfs_ino_t ino;
	/** File name. A null-terminated string. */
	char name[VSFS_NAME_MAX];

} vsfs_dentry;

static_assert(sizeof(vsfs_dentry) == 256, "invalid dentry size");
