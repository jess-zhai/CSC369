#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <stdarg.h>
#include <string.h>
#include "common.h"
#include "interrupt.h"

/* This is the function that will handle timer signals (i.e., the interrupt
 * handler). See 'man sigaction' for an explanation of the arguments.
 */
static void interrupt_handler(int sig, siginfo_t * sip, void *contextVP);

/* This function sets up a timer to deliver a signal to the process. 
 * These timer signals are the interrupts for the user-level threads.
 */
static void set_interrupt();

/* This function initializes signal set pointed to by setp so that only the 
 * signal used for the timer is included in the set.
 */
static void set_signal(sigset_t * setp);

static bool loud = false; /* print info from interrupt handler? */ 

/* Test programs will call this function after initializing the threads package.
 * Many of the calls won't make sense at first -- study the man pages! 
 */
void
register_interrupt_handler(bool verbose)
{
	struct sigaction action;
	int error;
	static bool init = false;

	assert(!init);	/* should only register once */
	init = true;
	loud = verbose;
	action.sa_handler = NULL;
	action.sa_sigaction = interrupt_handler; 
	
	/* The sa_mask field of struct sigaction specifies signals that 
	 * should be blocked while the signal handler is running. 
	 * The default behavior is to add the signal that triggered the 
	 * handler to this set, so it will be blocked while the handler runs.
	 * This prevents recursive interrupts, where another interrupt occurs 
	 * before the previous interrupt handler has finished running. 
	 */

	/* Initialize sa_mask to the empty set. This will allow all signals 
	 * except SIG_TYPE to be delivered while the signal handler is 
	 * running. For example, you can use ctrl-c to kill the process. 
	 */
	error = sigemptyset(&action.sa_mask);
	assert(!error);

	/* Use sa_sigaction field as handler instead of sa_handler field. */
	action.sa_flags = SA_SIGINFO;

	/* Install the signal handler. */
	if (sigaction(SIG_TYPE, &action, NULL)) {
		perror("Setting up signal handler");
		assert(0);
	}

	/* Initialize the timer. */
	set_interrupt();
}

/* Enables interrupts. */
bool
interrupts_on()
{
	return interrupts_set(true);
}

/* Disables interrupts. */
bool
interrupts_off()
{
	return interrupts_set(false);
}

/* Enables or disables interrupts, and returns whether interrupts were enabled
 * or not previously. 
 */
bool
interrupts_set(bool enable)
{
	int ret;
	sigset_t mask, omask;

	set_signal(&mask);
	
	if (enable) {
		ret = sigprocmask(SIG_UNBLOCK, &mask, &omask);
	} else {
		ret = sigprocmask(SIG_BLOCK, &mask, &omask);
	}
	assert(!ret);

	return (sigismember(&omask, SIG_TYPE) ? false : true);
}

/* Returns whether interrupts are currently enabled or not. */
bool
interrupts_enabled()
{
	sigset_t mask;
	int ret;

	ret = sigprocmask(0, NULL, &mask);
	assert(!ret);
	return (sigismember(&mask, SIG_TYPE) ? false : true);
}

/* Disables output from interrupt handler function. */
void
interrupts_quiet()
{
	loud = false;
}

/* Enables output from interrupt handler function. */
void
interrupts_loud()
{
	loud = true;
}


/* Turn off interrupts while printing. */
int
unintr_printf(const char *fmt, ...)
{
	int ret;
	bool enabled;
	va_list args;

	enabled = interrupts_off();
	va_start(args, fmt);
	ret = vprintf(fmt, args);
	va_end(args);
	interrupts_set(enabled);
	return ret;
}

/* static functions */

/* This function initializes signal set pointed to by setp so that only the 
 * signal used for the timer is included in the set.
 */
static void
set_signal(sigset_t * setp)
{
	int ret;
	ret = sigemptyset(setp);
	assert(!ret);
	ret = sigaddset(setp, SIG_TYPE);
	assert(!ret);
	return;
}

static bool first = true;
static struct timespec start, end, diff = { 0, 0 };

/*
 * Once register_interrupt_handler() is called, this routine gets called
 * each time the signal SIG_TYPE is sent to this process. 
 */
static void
interrupt_handler(int sig, siginfo_t * sip, void *contextVP)
{
	ucontext_t *context = (ucontext_t *) contextVP;

	/* Check that SIG_TYPE is blocked on entry. 
	 * This signal should be blocked because of the default signal 
	 * handling behavior. */
	assert(!interrupts_enabled());

	if (loud) {
		int ret;

		ret = clock_gettime(CLOCK_REALTIME, &end);
		assert(!ret);
		if (first) {
			first = 0;
		} else {
			diff = timespec_sub(&end, &start);
		}
		start = end;
		/* The printf() function is not safe to use in signal handlers.
		 * It is often used in example code, however, for convenience.
		 * The safe method for output in signal handlers is to 
		 * put the formatted output into a local buffer, and then use
		 * the write() system call to output that buffer to the chosen
		 * file descriptor. 
		 */
		char msgbuf[80];
		snprintf(msgbuf, 80, 
			 "%s: context at %10p, time diff = %ld us\n",
			 __FUNCTION__, context,
			 (diff.tv_sec * NSEC_PER_SEC + diff.tv_nsec)/1000);
		write(0, msgbuf, strlen(msgbuf));
	}

	/* Re-arm the timer to deliver the next interrupt */
	set_interrupt();
	
	/* Implement preemptive threading by calling thread_yield. */
	thread_yield(THREAD_ANY);
}

/*
 * Use the setitimer() system call to set an alarm in the future. At that time,
 * this process will receive a SIGALRM signal.
 *
 * In interrupt.h, we #define SIG_TYPE to the signal generated by the timer. 
 * Different timers may generate different signals, so using SIG_TYPE lets us
 * experiment with the timers here, without needing to change the other code
 * that deals with the signal delivered for timer interrupts. 
 */
static void
set_interrupt()
{
	int ret;
	struct itimerval val;

	/* QUESTION: Will the timer automatically fire every SIG_INTERVAL
	 * microseconds or not? (HINT: Read the man page for setitimer.)
	 */
	val.it_interval.tv_sec = 0;
	val.it_interval.tv_usec = 0;

	val.it_value.tv_sec = 0;
	val.it_value.tv_usec = SIG_INTERVAL;

	ret = setitimer(ITIMER_REAL, &val, NULL);
	assert(!ret);
}
