#ifndef __TIMER_H__
#define __TIMER_H__

#include <time.h>


// Returns current time in seconds since the Epoch as a floating point number
static inline double get_time()
{
   struct timespec t;
   clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
   return t.tv_sec + t.tv_nsec / 1000000000.0;
}


#endif /* __TIMER_H__ */
