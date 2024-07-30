/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Andrew Peterson, Karen Reid, Alexey Khrabrov, Angela Brown, Kuei Sun
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019, 2021 Karen Reid
 * Copyright (c) 2023, Angela Brown, Kuei Sun
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "malloc369.h"
#include "sim.h"
#include "coremap.h"
#include "swap.h"
#include "pagetable.h"

// Counters for various events.
// Your code must increment these when the related events occur.
size_t hit_count = 0;
size_t miss_count = 0;
size_t ref_count = 0;
size_t evict_clean_count = 0;
size_t evict_dirty_count = 0;

// Accessor functions for page table entries, to allow replacement
// algorithms to obtain information from a PTE, without depending
// on the internal implementation of the structure.

/* Returns true if the pte is marked valid, otherwise false */
bool is_valid(pt_entry_t *pte)
{
    return pte->valid;
}

/* Returns true if the pte is marked dirty, otherwise false */
bool is_dirty(pt_entry_t *pte)
{
    return pte->dirty;
}

/* Returns true if the pte is marked referenced, otherwise false */
bool get_referenced(pt_entry_t *pte)
{
    return pte->referenced;
}

/* Sets the 'referenced' status of the pte to the given val */
void set_referenced(pt_entry_t *pte, bool val)
{
    pte->referenced = val;
}

typedef struct
{
    pt_entry_t* entries[4096];
} page_table;

typedef struct
{
    page_table* tables[4096];
} page_dir;

page_dir* top_dir[4096];
/*
 * Initializes your page table.
 * This function is called once at the start of the simulation.
 * For the simulation, there is a single "process" whose reference trace is
 * being simulated, so there is just one overall page table.
 *
 * In a real OS, each process would have its own page table, which would
 * need to be allocated and initialized as part of process creation.
 *
 * The format of the page table, and thus what you need to do to get ready
 * to start translating virtual addresses, is up to you.
 */
void init_pagetable(void)
{
    
}

/*
 * Write virtual page represented by pte to swap, if needed, and update
 * page table entry.
 *
 * Called from allocate_frame() in coremap.c after a victim page frame has
 * been selected.
 *
 * Counters for evictions should be updated appropriately in this function.
 */
void handle_evict(pt_entry_t * pte)
{
    if (pte->dirty) {
        pte->offset = swap_pageout(pte->frame_num, pte->offset);
        pte->dirty = false;
        evict_dirty_count++;
    } else {
        evict_clean_count++;
    }
    pte->valid = false;
    pte->on_swap = true;
}

/*
 * Locate the physical frame number for the given vaddr using the page table.
 *
 * If the page table entry is invalid and not on swap, then this is the first
 * reference to the page and a (simulated) physical frame should be allocated
 * and initialized to all zeros (using init_frame from coremap.c).
 * If the page table entry is invalid and on swap, then a (simulated) physical
 * frame should be allocated and filled by reading the page data from swap.
 *
 * Make sure to update page table entry status information:
 *  - the page table entry should be marked valid
 *  - if the type of access is a write ('S'tore or 'M'odify),
 *    the page table entry should be marked dirty
 *  - a page should be marked dirty on the first reference to the page,
 *    even if the type of access is a read ('L'oad or 'I'nstruction type).
 *  - DO NOT UPDATE the page table entry 'referenced' information. That
 *    should be done by the replacement algorithm functions.
 *
 * When you have a valid page table entry, return the page frame number
 * that holds the requested virtual page.
 *
 * Counters for hit, miss and reference events should be incremented in
 * this function.
 */
int find_frame_number(vaddr_t vaddr, char type)
{
    unsigned long top_index = (vaddr >> 36) & 0xFFF;
    unsigned long middle_index = (vaddr >> 24) & 0xFFF;
    unsigned long bottom_index = (vaddr >> 12) & 0xFFF;
    if (top_dir[top_index] == NULL) {
        top_dir[top_index] = (page_dir*) malloc369(sizeof(page_dir));
    }

    page_table** middle_table = top_dir[top_index]->tables;
    if (middle_table[middle_index] == NULL) {
        middle_table[middle_index] = (page_table*) malloc369(sizeof(page_table));
    }
    pt_entry_t** bottom_entries = middle_table[middle_index]->entries;
    if (bottom_entries[bottom_index] == NULL) {
        bottom_entries[bottom_index] = (pt_entry_t*) malloc369(sizeof(pt_entry_t));
        bottom_entries[bottom_index]->valid = false;
        bottom_entries[bottom_index]->dirty = true;
        bottom_entries[bottom_index]->on_swap = false;
        bottom_entries[bottom_index]->referenced = false;
        bottom_entries[bottom_index]->frame_num = -1;
        bottom_entries[bottom_index]->offset = INVALID_SWAP;
    }
    pt_entry_t* target = bottom_entries[bottom_index];
    if(type == 'S' || type == 'M'){
        target->dirty = true;
    }
    if(target->valid){
        hit_count++;
        ref_count++;
        return target->frame_num;
    }
    if (!target->on_swap){
        target->frame_num = allocate_frame(target);
        init_frame(target->frame_num);
        target->dirty = true;
    } else {
        target->frame_num = allocate_frame(target);
        swap_pagein(target->frame_num, target->offset);
        target->on_swap = false;
    }
    target->valid = true;
    ref_count++;
    miss_count++;
    //printf("target status: \n frame: %d, onswap: %d, valid: %d, dirty: %d, ref: %d, offset: %ld\n",
    //target->frame_num, (int)target->on_swap, (int)target->valid, (int)target->dirty, (int)target->referenced, (long int)target->offset);
    return target->frame_num;
}


void print_pagetable(void)
{
    for (int i = 0; i < 4096; i++){
        if (top_dir[i]){
            page_dir* dir = top_dir[i];
            for (int j = 0; j < 4096; j++){
                if (dir->tables[j]){
                    page_table* table = dir->tables[j];
                    for (int k = 0; k < 4096; k++){
                        if (table->entries[k] && (table->entries[k]->valid || table->entries[k]->on_swap)){
                            printf("entry. frame: %d, offset: %ld. \n", table->entries[k]->frame_num, table->entries[k]->offset);
                        }
                    }
                }
            }
        }
    }
}


void free_pagetable(void)
{
    for (int i = 0; i < 4096; i++){
        if (top_dir[i]){
            page_dir* dir = top_dir[i];
            for (int j = 0; j < 4096; j++){
                if (dir->tables[j]){
                    page_table* table = dir->tables[j];
                    for (int k = 0; k < 4096; k++){
                        if (table->entries[k]){
                            free369(table->entries[k]);
                        }
                    }
                    free369(table);
                }
            }
            free369(dir);
        }
    }
    
}
