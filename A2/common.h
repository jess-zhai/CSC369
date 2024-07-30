#ifndef _COMMON_H_
#define _COMMON_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <signal.h>
#include "thread.h"


#define MSEC_PER_SEC 1000
#define USEC_PER_SEC 1000000
#define NSEC_PER_SEC 1000000000

/* Returns the result of a - b as a struct timespec. */
extern struct timespec timespec_sub(const struct timespec *a, const struct timespec *b);

/* Delay (busy wait) for the given number of microseconds */ 
extern void spin(unsigned long usecs);

/* Try to catch common errors to help reporting in the tester. */
extern void install_fatal_handlers(void *base);
extern void segfault_handler(int signum, siginfo_t *info, void *context);

#define TBD() do {							\
		printf("%s:%d: %s: please implement this functionality\n", \
		       __FILE__, __LINE__, __FUNCTION__);		\
		exit(1);						\
	} while (0)


#endif /* _COMMON_H_ */
