/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Alexey Khrabrov, Andrew Pelegris, Karen Reid
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019 Karen Reid
 */

/**
 * CSC369 Assignment 3 - Linked list header file.
 *
 * Circular doubly-linked list implementation. Adapted from the Linux kernel.
 *
 * NOTE: This file will be used with your code for marking, but our files 
 *       will use another file identical to the starter code in this
 *       version of list.h. 
 * 
 *       You are allowed to change or add code in this file, including 
 *       replacing the list  implementation with your own if you prefer. 
 *
 *       IMPORTANT: the 'struct frame' in coremap.h includes a 'list_entry' field. 
 *       You are not allowed to change coremap.h, so you must keep the definition
 *       of a 'list_entry' type.
 */

#pragma once

#include <stddef.h>

//NOTE: This linked list implementation is different from what you might be used
// to. To make a linked list of items of type struct S, instead of storing a
// struct S* pointer in the list entry struct, we embed a struct with the next
// and prev pointers in struct S (as one of its fields).
//
// Providing and managing storage for list entries is the responsibility of the
// user of this API. Keep in mind that an entry allocated on the stack (a local
// variable in a function) becomes invalid when the function returns.


/**
 * A node in a doubly-linked list.
 *
 * Should be a member of the struct that is linked into a list.
 */
typedef struct list_entry {
	struct list_entry *next;
	struct list_entry *prev;
} list_entry;

/**
 * Cast a member of a structure out to the containing structure.
 *
 * Useful for getting a pointer to the structure containing an embedded list
 * entry given a pointer to the embedded list entry.
 *
 * @param   ptr pointer to the member.
 * @param   type type of the container struct this is embedded in.
 * @param   member name of the member within the struct.
 * @return  pointer to the containing struct.
 */
#define container_of(ptr, type, member) ({            \
	const typeof(((type*)0)->member) *__mptr = (ptr); \
	(type*)((char*)__mptr - offsetof(type, member));  \
})

/** Doubly-linked list head. */
typedef struct list_head {
	struct list_entry head;
} list_head;


/** Initialize a list head. */
static inline void list_init(list_head *list)
{
	list_entry *head = &list->head;
	head->next = head;
	head->prev = head;
}

/** Destroy a list head. */
static inline void list_destroy(list_head *list)
{
	(void)list;
}

/**
 * Initialize a list entry into the unlinked state.
 *
 * NULL value in the next and prev fields means that the entry is not linked
 * into any list.
 *
 * @param entry  pointer to the list entry.
 */
static inline void list_entry_init(list_entry *entry)
{
	entry->next = NULL;
	entry->prev = NULL;
}

/** Determine if a list entry is linked into any list. */
static inline bool list_entry_is_linked(list_entry *entry)
{
	return entry->next != NULL;
}

/** Insert a new list entry between two known consecutive entries. */
static inline void __list_insert(list_entry *entry,
                                 list_entry *prev, list_entry *next)
{
	next->prev = entry;
	entry->next = next;
	entry->prev = prev;
	prev->next = entry;
}

/**
 * Add a new entry at the head of the list.
 *
 * Insert a new entry after the specified head. Useful for implementing stacks.
 *
 * @param list   pointer to the list head.
 * @param entry  pointer to the new entry to be added.
 */
static inline void list_add_head(list_head *list, list_entry *entry)
{
	__list_insert(entry, &list->head, list->head.next);
}

/**
 * Add a new entry at the tail of the list.
 *
 * Insert a new entry before the specified head. Useful for implementing queues.
 *
 * @param list   pointer to the list head.
 * @param entry  pointer to the new entry to be added.
 */
static inline void list_add_tail(list_head *list, list_entry *entry)
{
	__list_insert(entry, list->head.prev, &list->head);
}

/**
 * Delete an entry from a list.
 *
 * @param entry  pointer to the entry to delete from the list.
 */
static inline void list_del(list_entry *entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;

	list_entry_init(entry);
}


/**
 * list_first_entry - get a pointer to the first element on a list
 * @list:	the list to get the element pointer from.
 *
 * Note, the list may be empty and so the result may be NULL. 
 * Note2, this does not remove the element from the list.
 */
#define list_first_entry(list)  (list)->head.next


/**
 * list_last_entry - get a pointer to the last element on a list
 * @list:	the list to get the element pointer from.
 *
 * Note, the list may be empty and so the result may be NULL. 
 * Note2, this does not remove the element from the list.
 */
#define list_last_entry(list) (list)->head.prev


/**
 * Iterate over a list.
 *
 * NOTE: DO NOT attempt to modify the structure of the list while iterating
 * through it with this macro (you can still modify other fields of the struct
 * containing the list entry). If you delete the current list entry from the
 * list, pos->next will be invalidated, leading to a segmentation fault in
 * the next iteration.
 *
 * @param pos   struct list_entry pointer to use as a loop cursor.
 * @param list  pointer to the list.
 */
#define list_for_each(pos, list)					\
	for (pos = (list)->head.next; pos != &(list)->head; pos = pos->next)


