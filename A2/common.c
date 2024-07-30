#include <assert.h>
#include "common.h"
#include "interrupt.h"
#define NSEC_PER_SEC 1000000000

/* Returns the result of a - b as a struct timespec. */
struct timespec
timespec_sub(const struct timespec *a, const struct timespec *b)
{
	struct timespec r;
	r.tv_sec = a->tv_sec - b->tv_sec;
	r.tv_nsec = a->tv_nsec - b->tv_nsec;
	if (r.tv_nsec < 0) {
		/* borrow from secs */
		r.tv_sec--;
		r.tv_nsec += NSEC_PER_SEC;
	}
	return r;
}

/* Busy wait for the given number of microseconds */
void
spin(unsigned long usecs)
{
	struct timespec start, end, diff;
	int ret;
	unsigned long nsecs = usecs * 1000;

	ret = clock_gettime(CLOCK_REALTIME, &start);
	assert(!ret);
	while (1) {
		ret = clock_gettime(CLOCK_REALTIME, &end);
		diff = timespec_sub(&end, &start);
		if ((diff.tv_sec * NSEC_PER_SEC + diff.tv_nsec) >= nsecs) {
			break;
		}
	}
}

/*********** TRY TO CATCH SIGNALS FOR ERROR REPORTING ***************/

unsigned long start_addr; /* report error-causing %rip relative to this addr */

void segfault_handler(int signum, siginfo_t *info, void *context) {
	char msg[100];
	ucontext_t *uc = (ucontext_t *)context;
	unsigned long pc = uc->uc_mcontext.gregs[REG_RIP];
	
	snprintf(msg, 100,
		 "%s at instruction %lx (addr %p)\n\n",
		 strsignal(signum), pc, info->si_addr);
	fflush(0);
	write(0, msg, strlen(msg+1));
	if (signum != SIGABRT) {
		unsigned long bad_instr_offset = pc - start_addr + 0x1000;
		snprintf(msg, 100,
			 "try to run 'gdb --batch --ex=\"b *%lu\" <executable name>'\n\n",
			 bad_instr_offset);
		write(0, msg, strlen(msg+1));
	}
	exit(signum);
}

void install_fatal_handlers(void *base)
{
	struct sigaction sig_action;
	struct sigaction old_action;

	/* Very rough guess at start of code segment in memory */
	start_addr = (unsigned long)base & 0xfffffffffffff000;
	
	memset(&sig_action, 0, sizeof(sig_action));
	sig_action.sa_sigaction = segfault_handler;
	sig_action.sa_flags = SA_RESTART | SA_SIGINFO;
	sigemptyset(&sig_action.sa_mask);

	sigaction(SIGSEGV, &sig_action, &old_action);
	sigaction(SIGABRT, &sig_action, &old_action);
	sigaction(SIGTRAP, &sig_action, &old_action);
	sigaction(SIGILL, &sig_action, &old_action);
	sigaction(SIGFPE, &sig_action, &old_action);
}
