/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Alexey Khrabrov, Karen Reid
 *
 * This file is:
 * Copyright (c) 2019 Karen Reid
 */

/**
 * CSC369 Assignment 4 - File mapping helper header file.
 */

#pragma once

#include <stddef.h>


/**
 * Map the whole file into memory for reading and writing.
 *
 * File size must be a non-zero multiple of the block_size.
 *
 * @param path        image file path.
 * @param block_size  file system block size.
 * @param size        pointer to the variable that will be set to file size.
 * @return            pointer to the file mapping in memory on success;
 *                    NULL on failure.
 */
void *map_file(const char *path, size_t block_size, size_t *size);
