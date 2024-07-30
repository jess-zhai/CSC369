#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

//#include <stdio.h>
#include <signal.h>
#include <stdbool.h>

/* we will use this signal type for delivering "interrupts". */
#define SIG_TYPE SIGALRM
/* the interrupt will be delivered every 200 usec */
#define SIG_INTERVAL 200

void register_interrupt_handler(bool verbose);
bool interrupts_on(void);
bool interrupts_off(void);
bool interrupts_set(bool enable);
bool interrupts_enabled();
void interrupts_quiet();
void interrupts_loud();

/* turn off interrupts while printing */
int unintr_printf(const char *fmt, ...);
#endif
