#ifndef __MALLOC369_H__
#define __MALLOC369_H__

#include <stdbool.h>
#include <stddef.h>

/* malloc / free tracking functions */
extern long get_current_bytes_malloced();
extern long get_current_num_mallocs();
extern long get_num_mallocs();
extern long get_bytes_malloced();
extern bool is_leak_free();
extern void *malloc369(size_t size);
extern void free369(void *ptr);
extern void init_csc369_malloc(bool verbose);

#endif /* _MALLOC369_H__ */
