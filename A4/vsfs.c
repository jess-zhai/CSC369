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
 * CSC369 Assignment 4 - vsfs driver implementation.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

// Using 2.9.x FUSE API
#define FUSE_USE_VERSION 29
#include <fuse.h>

#include "vsfs.h"
#include "fs_ctx.h"
#include "options.h"
#include "util.h"
#include "bitmap.h"
#include "map.h"

//NOTE: All path arguments are absolute paths within the vsfs file system and
// start with a '/' that corresponds to the vsfs root directory.
//
// For example, if vsfs is mounted at "/tmp/my_userid", the path to a
// file at "/tmp/my_userid/dir/file" (as seen by the OS) will be
// passed to FUSE callbacks as "/dir/file".
//
// Paths to directories (except for the root directory - "/") do not end in a
// trailing '/'. For example, "/tmp/my_userid/dir/" will be passed to
// FUSE callbacks as "/dir".


/**
 * Initialize the file system.
 *
 * Called when the file system is mounted. NOTE: we are not using the FUSE
 * init() callback since it doesn't support returning errors. This function must
 * be called explicitly before fuse_main().
 *
 * @param fs    file system context to initialize.
 * @param opts  command line options.
 * @return      true on allocated; false on failure.
 */
static bool vsfs_init(fs_ctx *fs, vsfs_opts *opts)
{
	size_t size;
	void *image;
	
	// Nothing to initialize if only printing help
	if (opts->help) {
		return true;
	}

	// Map the disk image file into memory
	image = map_file(opts->img_path, VSFS_BLOCK_SIZE, &size);
	if (image == NULL) {
		return false;
	}

	return fs_ctx_init(fs, image, size);
}

/**
 * Cleanup the file system.
 *
 * Called when the file system is unmounted. Must cleanup all the resources
 * created in vsfs_init().
 */
static void vsfs_destroy(void *ctx)
{
	fs_ctx *fs = (fs_ctx*)ctx;
	if (fs->image) {
		munmap(fs->image, fs->size);
		fs_ctx_destroy(fs);
	}
}

/** Get file system context. */
static fs_ctx *get_fs(void)
{
	return (fs_ctx*)fuse_get_context()->private_data;
}


/* Returns the inode number for the element at the end of the path
 * if it exists.  If there is any error, return -1.
 * Possible errors include:
 *   - The path is not an absolute path
 *   - An element on the path cannot be found
 */
static int path_lookup(const char *path,  vsfs_ino_t *ino) {
	if(path[0] != '/') {
		fprintf(stderr, "Not an absolute path\n");
		return -1;
	} 

	// TODO: complete this function and any helper functions
	if (strcmp(path, "/") == 0) {
		*ino = VSFS_ROOT_INO;
		return 0;
	}
	fs_ctx *fs = get_fs();
	vsfs_inode* current_inode = &(fs->itable[VSFS_ROOT_INO]);

	for (int i = 0; i < VSFS_NUM_DIRECT; i++){
		if ((current_inode->i_direct[i] == VSFS_INO_MAX)){
			break;
		}
		vsfs_dentry* entry = (vsfs_dentry*) (fs->image + current_inode->i_direct[i] * VSFS_BLOCK_SIZE);
		for (int j = 0; j < (int) (VSFS_BLOCK_SIZE / sizeof(vsfs_dentry)); j++){
			if(entry[j].ino != VSFS_INO_MAX){
				if (strcmp(entry[j].name, path + 1) == 0){
					current_inode = &(fs->itable[entry[j].ino]);
					*ino = entry[j].ino;
					return 0;
				}
			}
		}
	}
	if (current_inode->i_indirect == 0){
		return -1;
	}
	vsfs_blk_t* base_indirect = (vsfs_blk_t*) (fs->image + current_inode->i_indirect * VSFS_BLOCK_SIZE);
	for (int i = 0; i < (int) (VSFS_BLOCK_SIZE / sizeof(vsfs_blk_t)); i++){
		if ((base_indirect[i] == VSFS_INO_MAX)){
			break;
		}
		vsfs_dentry* indirect_entry = (vsfs_dentry*) (fs->image + base_indirect[i] * VSFS_BLOCK_SIZE);
		for (int j = 0; j < (int) (VSFS_BLOCK_SIZE / sizeof(vsfs_dentry)); j++){
			if(indirect_entry[j].ino != VSFS_INO_MAX){
				if (strcmp(indirect_entry[j].name, path + 1) == 0){
					current_inode = &(fs->itable[indirect_entry[j].ino]);
					*ino = indirect_entry[j].ino;
					return 0;
				}
			}
		}
	}

	return -1;
}

/**
 * Get file system statistics.
 *
 * Implements the statvfs() system call. See "man 2 statvfs" for details.
 * The f_bfree and f_bavail fields should be set to the same value.
 * The f_ffree and f_favail fields should be set to the same value.
 * The following fields can be ignored: f_fsid, f_flag.
 * All remaining fields are required.
 *
 * Errors: none
 *
 * @param path  path to any file in the file system. Can be ignored.
 * @param st    pointer to the struct statvfs that receives the result.
 * @return      0 on allocated; -errno on error.
 */
static int vsfs_statfs(const char *path, struct statvfs *st)
{
	(void)path;// unused
	fs_ctx *fs = get_fs();
	vsfs_superblock *sb = fs->sb; /* Get ptr to superblock from context */
	
	memset(st, 0, sizeof(*st));
	st->f_bsize   = VSFS_BLOCK_SIZE;   /* Filesystem block size */
	st->f_frsize  = VSFS_BLOCK_SIZE;   /* Fragment size */
	// The rest of required fields are filled based on the information 
	// stored in the superblock.
        st->f_blocks = sb->sb_num_blocks;     /* Size of fs in f_frsize units */
        st->f_bfree  = sb->sb_free_blocks;    /* Number of free blocks */
        st->f_bavail = sb->sb_free_blocks;    /* Free blocks for unpriv users */
	st->f_files  = sb->sb_num_inodes;     /* Number of inodes */
        st->f_ffree  = sb->sb_free_inodes;    /* Number of free inodes */
        st->f_favail = sb->sb_free_inodes;    /* Free inodes for unpriv users */

	st->f_namemax = VSFS_NAME_MAX;     /* Maximum filename length */

	return 0;
}



/**
 * Get file or directory attributes.
 *
 * Implements the lstat() system call. See "man 2 lstat" for details.
 * The following fields can be ignored: st_dev, st_ino, st_uid, st_gid, st_rdev,
 *                                      st_blksize, st_atim, st_ctim.
 * All remaining fields are required.
 *
 * NOTE: the st_blocks field is measured in 512-byte units (disk sectors);
 *       it should include any metadata blocks that are allocated to the 
 *       inode (for vsfs, that is the indirect block). 
 *
 * NOTE2: the st_mode field must be set correctly for files and directories.
 *
 * Errors:
 *   ENAMETOOLONG  the path or one of its components is too long.
 *   ENOENT        a component of the path does not exist.
 *   ENOTDIR       a component of the path prefix is not a directory.
 *
 * @param path  path to a file or directory.
 * @param st    pointer to the struct stat that receives the result.
 * @return      0 on allocated; -errno on error;
 */
static int vsfs_getattr(const char *path, struct stat *st)
{
	if (strlen(path) >= VSFS_PATH_MAX) return -ENAMETOOLONG;
	memset(st, 0, sizeof(*st));
	vsfs_ino_t index;
	fs_ctx *fs = get_fs();
    if ((path_lookup(path, &index) < 0) | (index == VSFS_INO_MAX)) {
		return -ENOENT;
	}
	vsfs_inode* inode = &(fs->itable[index]);
	st->st_mode = inode->i_mode;
	st->st_size = inode->i_size;
	st->st_nlink = inode->i_nlink;
	st->st_blocks = inode->i_blocks * VSFS_BLOCK_SIZE / 512;
	if (inode->i_blocks > VSFS_NUM_DIRECT){
		st->st_blocks += VSFS_BLOCK_SIZE / 512;
	}
	st->st_mtim = inode->i_mtime;
	return 0; 

}

/**
 * Read a directory.
 *
 * Implements the readdir() system call. Should call filler(buf, name, NULL, 0)
 * for each directory direct_entry. See fuse.h in libfuse source code for details.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a directory.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a filler() call failed).
 *
 * @param path    path to the directory.
 * @param buf     buffer that receives the result.
 * @param filler  function that needs to be called for each directory direct_entry.
 *                Pass 0 as offset (4th argument). 3rd argument can be NULL.
 * @param offset  unused.
 * @param fi      unused.
 * @return        0 on allocated; -errno on error.
 */
static int vsfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi)
{
	(void)offset;// unused
	(void)fi;// unused
	fs_ctx *fs = get_fs();

	vsfs_ino_t index;
	if (path_lookup(path, &index) < 0){
		return -1;
	}
	vsfs_inode * inode = &(fs->itable[index]);
	int entries = div_round_up(inode->i_size, sizeof(vsfs_dentry));
	int num_blocks = inode->i_blocks;
	int dentry_per_block = VSFS_BLOCK_SIZE / sizeof(vsfs_dentry);
	for (int i = 0; i < VSFS_NUM_DIRECT; i++){
		if (num_blocks == 0){
			break;
		}
		vsfs_dentry * direct_entry = (vsfs_dentry *) (fs->image + inode->i_direct[i] * VSFS_BLOCK_SIZE);
		for (int j = 0; j < dentry_per_block; j++){

			if (direct_entry[j].ino != VSFS_INO_MAX) {
				if (filler(buf, direct_entry[j].name, NULL, 0) != 0){
					return -ENOMEM;
				}
				entries--;
				if (entries == 0) {
					break;
				}
			}
		}
		num_blocks--;
	}
	// inode has more than VSFS_NUM_DIRECT blocks, check indirect block
	if (num_blocks > 0){
		vsfs_blk_t * base_indirect = (vsfs_blk_t *) (fs->image + inode->i_indirect * VSFS_BLOCK_SIZE);
		for (int i = 0; i < num_blocks; i++){
			vsfs_dentry * direct_entry = (vsfs_dentry *) (fs->image + base_indirect[i] * VSFS_BLOCK_SIZE);
			for (int j = 0; j < dentry_per_block; j++){

				if (direct_entry[j].ino != VSFS_INO_MAX) {
					if (filler(buf, direct_entry[j].name, NULL, 0) != 0){
						return -ENOMEM;
					}
					entries--;
					if (entries == 0) {
						return 0;
					}
				}
			}
		}
	}
	return 0;
}


/**
 * Create a file.
 *
 * Implements the open()/creat() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" doesn't exist.
 *   The parent directory of "path" exists and is a directory.
 *   "path" and its components are not too long.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the file to create.
 * @param mode  file mode bits.
 * @param fi    unused.
 * @return      0 on allocated; -errno on error.
 */
static int vsfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	(void)fi;// unused
	assert(S_ISREG(mode));
	fs_ctx *fs = get_fs();

	//TODO: create a file at given path with given mode
	char *path_copy = strdup(path);
    char *end_path = strtok(path_copy, "/");
    vsfs_inode *parent = &fs->itable[VSFS_ROOT_INO];

    // Determine the last block of the parent directory
    vsfs_blk_t *block_ptr = parent->i_blocks <= VSFS_NUM_DIRECT ? &parent->i_direct[parent->i_blocks - 1] : &((vsfs_blk_t *)(fs->image + parent->i_indirect * VSFS_BLOCK_SIZE))[parent->i_blocks - VSFS_NUM_DIRECT - 1];
    vsfs_dentry *last_block = (vsfs_dentry *)(fs->image + (*block_ptr) * VSFS_BLOCK_SIZE);

    int i, entry_per_block = VSFS_BLOCK_SIZE / sizeof(vsfs_dentry);
    vsfs_dentry *new_entry = NULL;

    // Find an available direct_entry in the last block
    for (i = 0; i < entry_per_block && !new_entry; i++) {
        if (last_block[i].ino == VSFS_INO_MAX) {
            new_entry = &last_block[i];
        }
    }
    // Handle full block or indirect block case
    if (!new_entry) {
        if (parent->i_blocks == VSFS_NUM_DIRECT + VSFS_BLOCK_SIZE / sizeof(vsfs_blk_t)) {
            return -ENOSPC; // Filesystem is full
        }

        vsfs_blk_t *new_block_ptr = parent->i_blocks < VSFS_NUM_DIRECT ? &parent->i_direct[parent->i_blocks] : &((vsfs_blk_t *)(fs->image + parent->i_indirect * VSFS_BLOCK_SIZE))[parent->i_blocks - VSFS_NUM_DIRECT];
        if (bitmap_alloc(fs->dbmap, fs->sb->sb_num_blocks, new_block_ptr) != 0) {
            return -ENOSPC;
        }
        vsfs_dentry *new_block = (vsfs_dentry *)(fs->image + (*new_block_ptr) * VSFS_BLOCK_SIZE);
        for (int j = 0; j < entry_per_block; j++) {
            new_block[j].ino = VSFS_INO_MAX;
        }

        new_entry = &new_block[0];
        parent->i_blocks++;
        parent->i_size += VSFS_BLOCK_SIZE;
        fs->sb->sb_free_blocks--;
    }

    // Update directory modification time
    if (clock_gettime(CLOCK_REALTIME, &parent->i_mtime) != 0) {
        perror("clock_gettime");
    }

    // Allocate a new inode
    if (bitmap_alloc(fs->ibmap, fs->sb->sb_num_inodes, &new_entry->ino) != 0) {
        return -ENOSPC;
    }
    fs->sb->sb_free_inodes--;
    strncpy(new_entry->name, end_path, VSFS_NAME_MAX);
    vsfs_inode *new_inode = &fs->itable[new_entry->ino];

    // Initialize the new inode
    new_inode->i_mode = mode;
    new_inode->i_size = 0;
    new_inode->i_blocks = 0;
    new_inode->i_nlink = 1;
    if (clock_gettime(CLOCK_REALTIME, &new_inode->i_mtime) != 0) {
        perror("clock_gettime");
    }

    return 0;
}

/**
 * Remove a file.
 *
 * Implements the unlink() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors: none
 *
 * @param path  path to the file to remove.
 * @return      0 on allocated; -errno on error.
 */
static int vsfs_unlink(const char *path)
{
	fs_ctx *fs = get_fs();

	//TODO: remove the file at given path
	vsfs_ino_t index;

    // Lookup path to get index
    if (path_lookup(path, &index) < 0) {
        return -ENOENT;
    }

    vsfs_inode* target_node = &(fs->itable[index]);

    // Free blocks associated with the inode
    for (vsfs_blk_t i = 0; i < target_node->i_blocks; i++) {
        vsfs_blk_t block_index = (i < VSFS_NUM_DIRECT) ? target_node->i_direct[i] : *((vsfs_blk_t *)(fs->image + target_node->i_indirect * VSFS_BLOCK_SIZE) + i - VSFS_NUM_DIRECT);
        bitmap_free(fs->dbmap, fs->sb->sb_num_blocks, block_index);
        fs->sb->sb_free_blocks++;
    }

    // Free indirect block if it exists
    if (target_node->i_blocks > VSFS_NUM_DIRECT) {
        bitmap_free(fs->dbmap, fs->sb->sb_num_blocks, target_node->i_indirect);
        fs->sb->sb_free_blocks++;
    }

    // Reset inode properties
    for (int i = 0; i < VSFS_NUM_DIRECT; i++) {
        target_node->i_direct[i] = 0;
    }
    target_node->i_indirect = 0;
    target_node->i_blocks = 0;
    target_node->i_size = 0;
    target_node->i_nlink = 0;

    // Free inode
    vsfs_ino_t index2;
    path_lookup(path, &index2);  // Assuming this call is required again
    bitmap_free(fs->ibmap, fs->sb->sb_num_inodes, index2);
    fs->sb->sb_free_inodes++;

    // Update root inode's modification time
    vsfs_inode *root = &fs->itable[VSFS_ROOT_INO];
    if (clock_gettime(CLOCK_REALTIME, &root->i_mtime) != 0) {
        perror("clock_gettime");
    }

    // Update directory direct_entry
    vsfs_dentry *direct_entry = (vsfs_dentry *)(fs->image + root->i_direct[0] * VSFS_BLOCK_SIZE);
    for (vsfs_blk_t i = 0; i < VSFS_BLOCK_SIZE / sizeof(vsfs_dentry); i++) {
        if (direct_entry[i].ino == index2) {
            direct_entry[i].ino = VSFS_INO_MAX;
            break;
        }
    }
    return 0;
}


/**
 * Change the modification time of a file or directory.
 *
 * Implements the utimensat() system call. See "man 2 utimensat" for details.
 *
 * NOTE: You only need to implement the setting of modification time (mtime).
 *       Timestamp modifications are not recursive. 
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists.
 *
 * Errors: none
 *
 * @param path   path to the file or directory.
 * @param times  timestamps array. See "man 2 utimensat" for details.
 * @return       0 on allocated; -errno on failure.
 */
static int vsfs_utimens(const char *path, const struct timespec times[2])
{
	fs_ctx *fs = get_fs();
	vsfs_inode *ino = NULL;
	
	//TODO: update the modification timestamp (mtime) in the inode for given
	// path with either the time passed as argument or the current time,
	// according to the utimensat man page
	
	// 0. Check if there is actually anything to be done.
	if (times[1].tv_nsec == UTIME_OMIT) {
		// Nothing to do.
		return 0;
	}

	// 1. TODO: Find the inode for the final component in path
	vsfs_ino_t index;
	if (path_lookup(path, &index) < 0)
	{
		return -ENOENT;
	}
	ino = &(fs->itable[index]);
	
	// 2. Update the mtime for that inode.
	//    This code is commented out to avoid failure until you have set
	//    'ino' to point to the inode structure for the inode to update.
	if (times[1].tv_nsec == UTIME_NOW) {
		if (clock_gettime(CLOCK_REALTIME, &(ino->i_mtime)) != 0) {
			// clock_gettime should not fail, unless you give it a
			// bad pointer to a timespec.
			assert(false);
		}
	} else {
		ino->i_mtime = times[1];
	}

	return 0;
	//return -ENOSYS;
}

static inline int min(int a, int b) {
    return (a < b) ? a : b;
}

static int allocate_new_blocks(fs_ctx *fs, vsfs_inode *inode, uint32_t num_blocks) {
    uint32_t old_blocks = div_round_up(inode->i_size, VSFS_BLOCK_SIZE);
    int blocks_needed = num_blocks - old_blocks;
    assert(blocks_needed >= 0);

    int direct_block_needed = (inode->i_blocks < VSFS_NUM_DIRECT) ? min(blocks_needed, VSFS_NUM_DIRECT - inode->i_blocks) : 0;
    int indirect_block_needed = blocks_needed - direct_block_needed;

    // Allocate direct blocks
    for (int i = 0; i < direct_block_needed; i++) {
        if (bitmap_alloc(fs->dbmap, fs->sb->sb_num_blocks, &inode->i_direct[inode->i_blocks]) != 0) {
            return -ENOSPC;
        }
        fs->sb->sb_free_blocks--;
        memset(fs->image + inode->i_direct[inode->i_blocks] * VSFS_BLOCK_SIZE, 0, VSFS_BLOCK_SIZE);
        inode->i_blocks++;
    }

    // Allocate indirect blocks
    if (indirect_block_needed > 0) {
        if (inode->i_blocks == VSFS_NUM_DIRECT) {
            if (bitmap_alloc(fs->dbmap, fs->sb->sb_num_blocks, &inode->i_indirect) != 0) {
                return -ENOSPC;
            }
            fs->sb->sb_free_blocks--;
        }
        vsfs_blk_t *indirect_block = (vsfs_blk_t *)(fs->image + inode->i_indirect * VSFS_BLOCK_SIZE);
        for (int i = 0; i < indirect_block_needed; i++) {
            if (bitmap_alloc(fs->dbmap, fs->sb->sb_num_blocks, &indirect_block[inode->i_blocks - VSFS_NUM_DIRECT]) != 0) {
                return -ENOSPC;
            }
            fs->sb->sb_free_blocks--;
            memset(fs->image + indirect_block[inode->i_blocks - VSFS_NUM_DIRECT] * VSFS_BLOCK_SIZE, 0, VSFS_BLOCK_SIZE);
            inode->i_blocks++;
        }
    }

    return 0;
}

static int free_excess_blocks(fs_ctx *fs, vsfs_inode *inode, uint32_t num_blocks) {
    uint32_t blocks_to_free = inode->i_blocks - num_blocks;
    vsfs_blk_t *indirect_block = (inode->i_blocks > VSFS_NUM_DIRECT) ? (vsfs_blk_t *)(fs->image + inode->i_indirect * VSFS_BLOCK_SIZE) : NULL;

    // Free indirect blocks
    for (uint32_t i = 0; i < blocks_to_free && inode->i_blocks > VSFS_NUM_DIRECT; i++) {
        vsfs_blk_t block_index = indirect_block[inode->i_blocks - VSFS_NUM_DIRECT - 1];
        bitmap_free(fs->dbmap, fs->sb->sb_num_blocks, block_index);
        indirect_block[inode->i_blocks - VSFS_NUM_DIRECT - 1] = 0;
        inode->i_blocks--;
        fs->sb->sb_free_blocks++;
    }

    // Free the indirect block if it is no longer used
    if (inode->i_blocks == VSFS_NUM_DIRECT && inode->i_indirect != 0) {
        bitmap_free(fs->dbmap, fs->sb->sb_num_blocks, inode->i_indirect);
        inode->i_indirect = 0;
        fs->sb->sb_free_blocks++;
    }

    // Free direct blocks
    for (uint32_t i = 0; i < blocks_to_free && inode->i_blocks <= VSFS_NUM_DIRECT; i++) {
        bitmap_free(fs->dbmap, fs->sb->sb_num_blocks, inode->i_direct[inode->i_blocks - 1]);
        inode->i_direct[inode->i_blocks - 1] = 0;
        inode->i_blocks--;
        fs->sb->sb_free_blocks++;
    }

    return 0;
}
/**
 * Change the size of a file.
 *
 * Implements the truncate() system call. Supports both extending and shrinking.
 * If the file is extended, the new uninitialized range at the end must be
 * filled with zeros.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *   EFBIG   write would exceed the maximum file size. 
 *
 * @param path  path to the file to set the size.
 * @param size  new file size in bytes.
 * @return      0 on allocated; -errno on error.
 */
static int vsfs_truncate(const char *path, off_t size)
{
	fs_ctx *fs = get_fs();

	//TODO: set new file size, possibly "zeroing out" the uninitialized range
	vsfs_ino_t index;
	if (path_lookup(path, &index) < 0){
		return -1;
	}
	vsfs_inode* inode = &(fs->itable[index]);

    if ((uint32_t)size > (VSFS_NUM_DIRECT + (VSFS_BLOCK_SIZE / sizeof(vsfs_blk_t))) * VSFS_BLOCK_SIZE) {
        return -EFBIG;
    }
    uint32_t old_size = inode->i_size;
    uint32_t num_blocks = div_round_up(size, VSFS_BLOCK_SIZE);
    uint32_t old_blocks = inode->i_blocks;

    if (old_size == size) {
        // No change in size
        return 0;
    }

    if (old_size < size) {
        // Expanding the file size
        if (num_blocks > old_blocks) {
            if (allocate_new_blocks(fs, inode, num_blocks) != 0) {
                return -ENOSPC;
            }
        }
    } else {
        // Shrinking the file size
        if (free_excess_blocks(fs, inode, num_blocks) != 0) {
            return -ENOSPC;
        }
    }

    inode->i_size = size;
    if (clock_gettime(CLOCK_REALTIME, &(inode->i_mtime)) != 0) {
        perror("clock_gettime");
    }

    return 0;
}


/**
 * Read data from a file.
 *
 * Implements the pread() system call. Must return exactly the number of bytes
 * requested except on EOF (end of file). Reads from file ranges that have not
 * been written to must return ranges filled with zeros. You can assume that the
 * byte range from offset to offset + size is contained within a single block.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors: none
 *
 * @param path    path to the file to read from.
 * @param buf     pointer to the buffer that receives the data.
 * @param size    buffer size (number of bytes requested).
 * @param offset  offset from the beginning of the file to read from.
 * @param fi      unused.
 * @return        number of bytes read on allocated; 0 if offset is beyond EOF;
 *                -errno on error.
 */
static int vsfs_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
	(void)fi;// unused
	fs_ctx *fs = get_fs();

	//TODO: read data from the file at given offset into the buffer
	vsfs_ino_t index;
	if (path_lookup(path, &index) < 0){
		return -1;
	}
	vsfs_inode* inode = &(fs->itable[index]);
	if ((vsfs_blk_t)offset >= inode->i_size) {
        return 0;
    } else if (offset + size > inode->i_size) {
        size = inode->i_size - offset;
    }
    if (size == 0) {
        return 0;
    }

    vsfs_blk_t start_block = offset / VSFS_BLOCK_SIZE;
    off_t start_block_offset = offset % VSFS_BLOCK_SIZE;
    vsfs_blk_t end_block = (offset + size) / VSFS_BLOCK_SIZE;
    off_t end_block_offset = (offset + size) % VSFS_BLOCK_SIZE;

    for (vsfs_blk_t i = start_block; i <= end_block; i++) {
        vsfs_blk_t block_index = i < VSFS_NUM_DIRECT ? inode->i_direct[i] : *((vsfs_blk_t *)(fs->image + inode->i_indirect * VSFS_BLOCK_SIZE) + i - VSFS_NUM_DIRECT);
        off_t read_offset = i == start_block ? start_block_offset : 0;
        size_t read_size = (i == start_block ? VSFS_BLOCK_SIZE - start_block_offset : (i == end_block ? end_block_offset : VSFS_BLOCK_SIZE));

        memcpy(buf + (i - start_block) * VSFS_BLOCK_SIZE - start_block_offset, fs->image + block_index * VSFS_BLOCK_SIZE + read_offset, read_size);
    }

    return size;

}

/**
 * Write data to a file.
 *
 * Implements the pwrite() system call. Must return exactly the number of bytes
 * requested except on error. If the offset is beyond EOF (end of file), the
 * file must be extended. If the write creates a "hole" of uninitialized data,
 * the new uninitialized range must filled with zeros. You can assume that the
 * byte range from offset to offset + size is contained within a single block.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *   EFBIG   write would exceed the maximum file size 
 *
 * @param path    path to the file to write to.
 * @param buf     pointer to the buffer containing the data.
 * @param size    buffer size (number of bytes requested).
 * @param offset  offset from the beginning of the file to write to.
 * @param fi      unused.
 * @return        number of bytes written on allocated; -errno on error.
 */
static int vsfs_write(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi)
{
	(void)fi;// unused
	fs_ctx *fs = get_fs();

	//TODO: write data from the buffer into the file at given offset, possibly

	vsfs_ino_t index;
	if (path_lookup(path, &index) < 0){
		return -1;
	}
	vsfs_inode *inode = &(fs->itable[index]);

    if (offset + size > (VSFS_NUM_DIRECT + VSFS_BLOCK_SIZE / sizeof(vsfs_blk_t)) * VSFS_BLOCK_SIZE) {
        return -EFBIG;
    }
    vsfs_blk_t old_size = inode->i_size;
    vsfs_blk_t old_blocks = inode->i_blocks;

    if ((vsfs_blk_t)(offset + size) >= inode->i_size || (offset + size) / VSFS_BLOCK_SIZE >= inode->i_blocks) {
        vsfs_truncate(path, offset + size);
    }

    if (size == 0) {
        return 0;
    }
    vsfs_blk_t write_start = offset / VSFS_BLOCK_SIZE;
    vsfs_blk_t write_end = (offset + size) / VSFS_BLOCK_SIZE;
    vsfs_blk_t start_offset = offset % VSFS_BLOCK_SIZE;
    vsfs_blk_t end_offset = (offset + size) % VSFS_BLOCK_SIZE;

    for (vsfs_blk_t i = write_start; i <= write_end; i++) {
        vsfs_blk_t block_index = i < VSFS_NUM_DIRECT ? inode->i_direct[i] : *((vsfs_blk_t *)(fs->image + inode->i_indirect * VSFS_BLOCK_SIZE) + i - VSFS_NUM_DIRECT);
        assert(block_index != 0);

        off_t block_offset = (i == write_start) ? start_offset : 0;
        size_t write_size = (i == write_start) ? VSFS_BLOCK_SIZE - start_offset : (i == write_end ? end_offset : VSFS_BLOCK_SIZE);

        memcpy(fs->image + block_index * VSFS_BLOCK_SIZE + block_offset, buf + (i - write_start) * VSFS_BLOCK_SIZE - start_offset, write_size);
    }
    // Handle the hole case
    if (old_size < offset) {
        memset(fs->image + inode->i_direct[old_blocks - 1] * VSFS_BLOCK_SIZE + old_size % VSFS_BLOCK_SIZE, 0, VSFS_BLOCK_SIZE - (old_size % VSFS_BLOCK_SIZE));
    }

    if (clock_gettime(CLOCK_REALTIME, &inode->i_mtime) != 0) {
        perror("clock_gettime");
    }

    return size;
}


static struct fuse_operations vsfs_ops = {
	.destroy  = vsfs_destroy,
	.statfs   = vsfs_statfs,
	.getattr  = vsfs_getattr,
	.readdir  = vsfs_readdir,
	.create   = vsfs_create,
	.unlink   = vsfs_unlink,
	.utimens  = vsfs_utimens,
	.truncate = vsfs_truncate,
	.read     = vsfs_read,
	.write    = vsfs_write,
};

int main(int argc, char *argv[])
{
	vsfs_opts opts = {0};// defaults are all 0
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	if (!vsfs_opt_parse(&args, &opts)) return 1;

	fs_ctx fs = {0};
	if (!vsfs_init(&fs, &opts)) {
		fprintf(stderr, "Failed to mount the file system\n");
		return 1;
	}

	return fuse_main(args.argc, args.argv, &vsfs_ops, &fs);
}