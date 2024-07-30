#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unordered_map>

unsigned long num_malloced;
unsigned long current_num_malloced;
unsigned long bytes_malloced;
unsigned long current_bytes_malloced;

std::unordered_map<void *, size_t> malloc_map;

extern "C" void * malloc_csc369(size_t size)
{
	num_malloced++;
	current_num_malloced++;
	bytes_malloced += size;
	current_bytes_malloced += size;
	void * m = malloc(size);
	if (m == NULL) {
		exit(-1);
	}
	malloc_map[m] = size;
	return m;
}

extern "C" void free_csc369(void * ptr)
{
	free(ptr);
	size_t size = malloc_map[ptr];
	current_num_malloced --;
	current_bytes_malloced -= size;
}

extern "C" void * realloc_csc369(void * ptr, size_t new_size)
{
	void * m = realloc(ptr, new_size);
	size_t size = malloc_map[ptr];
	current_bytes_malloced -= size;
	malloc_map[m] = new_size;
	current_bytes_malloced += new_size;
	return m;
}

extern "C" void init_csc369_malloc()
{
	num_malloced = 0;
	bytes_malloced = 0;
	current_num_malloced = 0;
	current_bytes_malloced = 0;
}

extern "C" unsigned long get_current_bytes_malloced()
{
	return current_bytes_malloced;
}

extern "C" unsigned long get_current_num_malloced()
{
	return current_num_malloced;
}

extern "C" unsigned long get_num_malloced()
{
	return num_malloced;
}

extern "C" unsigned long get_bytes_malloced()
{
	return bytes_malloced;
}


extern "C" bool is_leak_free()
{
	if (current_bytes_malloced == 0 && current_num_malloced == 0) {
		return true;
	} else {
		return false;
	}
}
