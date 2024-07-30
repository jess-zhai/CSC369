#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdio.h>
#include "thread.h"
#include <string.h>
// #define DEBUG_USE_VALGRIND

// #ifdef DEBUG_USE_VALGRIND
// #include <valgrind.h>
// #endif

/* This is the wait queue structure, needed for Assignment 2. */ 
struct wait_queue {
	/* ... Fill this in Assignment 2 ... */
};

/* For Assignment 1, you will need a queue structure to keep track of the 
 * runnable threads. You can use the tutorial 1 queue implementation if you 
 * like. You will probably find in Assignment 2 that the operations needed
 * for the wait_queue are the same as those needed for the ready_queue.
 */

/* This is the thread control block. */
struct thread {
	Tid thread_id;
	int state_id;
	ucontext_t * context;
	struct thread * next;
};
struct queue {
  	struct thread * head;
};

typedef enum
{
  empty = 0,
  ready = 1,
  running = 2,
  kill = 3,
  quit = 4 
} state;

int all_threads[THREAD_MAX_THREADS]; 
struct queue ready_queue;
struct thread * running_thread; 

/**************************************************************************
 * Assignment 1: Refer to thread.h for the detailed descriptions of the six
 *               functions you need to implement. 
 **************************************************************************/

// void print_queue(struct queue* queue){
// 	if (!queue->head){
// 		printf("Nothing in queue\n");
// 	} else {
// 		struct thread * curr = queue->head;
// 		while (curr->next != NULL){
// 			printf("[id: %d, state: %d] -> ", (int)curr->thread_id, (int)curr->state_id);
// 			curr = curr->next;
// 		}
// 		printf("queue end.\n");
// 	}
// }


void enqueue(struct thread * new_thread){
    if (!ready_queue.head){
        ready_queue.head = new_thread;
    } else {
        struct thread * curr = ready_queue.head;
        while (curr->next != NULL){
            curr = curr->next;
        }
        curr->next = new_thread;
    }
}

struct thread *dequeue(Tid thread_id){
  if (!ready_queue.head){
    return NULL;
  } else if (ready_queue.head->thread_id == thread_id){
    struct thread * target = ready_queue.head;
    ready_queue.head = target->next;
    target->next = NULL;
    return target;
  } else {
    struct thread * prev = ready_queue.head;
    struct thread * curr = ready_queue.head->next;
    while (curr->thread_id != thread_id){
      prev = prev->next;
      curr = curr->next;
    }
    if (curr) {
		prev->next = curr->next;
		curr->next = NULL;
		return curr;
    }
    return NULL;
  }
}

void
thread_init(void)
{
	/* Add necessary initialization for your threads library here. */
        /* Initialize the thread control block for the first thread */
	for (int i = 0; i < THREAD_MAX_THREADS; i++){
		all_threads[i] = 0;
	}
	ready_queue.head = NULL;
	// next we define the running thread.
	running_thread = (struct thread *) malloc_csc369(sizeof(struct thread));
	running_thread->thread_id = 0;
	running_thread->state_id = running; 
	running_thread->context = (ucontext_t *) malloc_csc369(sizeof(ucontext_t));
	running_thread->next = NULL;
	all_threads[0] = running_thread->state_id;
}

Tid
thread_id()
{
	if (running_thread == NULL){
		return THREAD_INVALID;
	}
	return running_thread->thread_id;
}

/* New thread starts by calling thread_stub. The arguments to thread_stub are
 * the thread_main() function, and one argument to the thread_main() function. 
 */
void
thread_stub(void (*thread_main)(void *), void *arg)
{
	thread_main(arg); // call thread_main() function with arg
	thread_exit(0);
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
	int id = -1;
	for (int i = 0; i < THREAD_MAX_THREADS; i++){
		if (all_threads[i] == 0){
			id = i;
			break;
		}
	}
	if (id == -1){
		return THREAD_NOMORE;
	}
	struct thread * new_thread = (struct thread*)malloc_csc369(sizeof(struct thread));
	if (new_thread == NULL){
		return THREAD_NOMEMORY;
	}
	new_thread->thread_id = id;
	new_thread->state_id = ready; 
	new_thread->context = (ucontext_t *) malloc_csc369(sizeof(ucontext_t));
	new_thread->next = NULL;
	all_threads[id] = new_thread->state_id; 
	if(getcontext(new_thread->context) == -1){
		return THREAD_FAILED;
	} 
	new_thread->context->uc_stack.ss_sp = (void *) malloc_csc369(THREAD_MIN_STACK);
	if (!new_thread->context->uc_stack.ss_sp){
		free_csc369(new_thread);
		return THREAD_NOMEMORY;
	}
	new_thread->context->uc_stack.ss_size = THREAD_MIN_STACK;
	new_thread->context->uc_mcontext.gregs[REG_RSP] = (unsigned long) 
	new_thread->context->uc_stack.ss_sp + new_thread->context->uc_stack.ss_size - sizeof(long);
	new_thread->context->uc_mcontext.gregs[REG_RIP] = (unsigned long) &thread_stub;
	new_thread->context->uc_mcontext.gregs[REG_RDI] = (unsigned long) fn;
	new_thread->context->uc_mcontext.gregs[REG_RSI] = (unsigned long) parg;
	enqueue(new_thread);
	// print_queue(&ready_queue);
	return id;
}

Tid
thread_yield(Tid want_tid)
{
	int contextSet = 0; // check if set context has run once already
	// printf("before zombie\n");
	// print_queue(&ready_queue);
	for (int i = 0; i < THREAD_MAX_THREADS; i++){
		if (all_threads[i] == quit && i != thread_id()){
		struct thread * curr = dequeue(i);
		free(curr->context->uc_stack.ss_sp);
		free(curr->context);
		free(curr);
		all_threads[i] = empty;
		}
	}
	// printf("after zombie\n");
	// print_queue(&ready_queue);
	Tid id;
	//printf("thread. want_tid: %d\n", (int) want_tid);
	if (want_tid == THREAD_SELF || want_tid == thread_id()){
		//printf("%d\n", (int)want_tid);
		return thread_id(); //yield to self
	} else if (want_tid == THREAD_ANY){ // yield to next available thread
		if (!ready_queue.head){
			return THREAD_NONE;
		}
		//printf("thread. in any, id to next ready thread\n");
		if (running_thread->state_id == kill){
			int exit_code = 0;
			thread_exit(exit_code);
		}
		id = ready_queue.head->thread_id;
		//printf("ok to proceed\n");
		if(getcontext(running_thread->context) == -1){
			return THREAD_FAILED;
		} 
		if (!contextSet){
			//printf("setting context to 1\n");
			contextSet = 1;
			Tid running_id = thread_id(); 
			if (all_threads[running_id] != quit && all_threads[running_id] != kill){
				running_thread->state_id = ready;
				all_threads[running_id] = running_thread->state_id;
			}
			enqueue(running_thread);
			// yield to thread that has corresponding thread_id
			running_thread = dequeue(id);
			if (all_threads[id] != quit && all_threads[id] != kill){
				running_thread->state_id = running;
				all_threads[id] = running_thread->state_id; //set it to running state
			}
			
			if(setcontext(running_thread->context) == -1){
				return THREAD_FAILED;
			};
		}
		// Tid now = thread_id();
		// printf("yielding to: %d\n", (int)now);
		return thread_id();

		//printf("thread. next ready id: %d\n", (int)id);
	} else if (want_tid < 0 || want_tid > THREAD_MAX_THREADS - 1){ //if specified tid and invalid
		return THREAD_INVALID;
	} else if (all_threads[want_tid] == empty){ 
		return THREAD_INVALID; 
	} else {
		//printf("in thread.c, after any or in else. yield to thread %d\n", (int)want_tid);
		id = want_tid;
		//printf("ok to proceed\n");
		if (running_thread->state_id == kill){
			int exit_code = 0;
			thread_exit(exit_code);
		}
		if(getcontext(running_thread->context) == -1){
			return THREAD_FAILED;
		} 
		if (!contextSet){
			//printf("setting context to 1\n");
			contextSet = 1;
			Tid running_id = thread_id(); 
			if (all_threads[running_id] != quit && all_threads[running_id] != kill){
				running_thread->state_id = ready;
				all_threads[running_id] = running_thread->state_id;
			}
			//printf("adding %d to ready queue\n", (int) running_thread->thread_id);
			enqueue(running_thread);
			running_thread = dequeue(id);
			//printf("dequed thread: %d\n", (int) thread_id());
			if (all_threads[id] != quit && all_threads[id] != kill){
				running_thread->state_id = running;
				//printf("in if, now running\n");
				all_threads[id] = running_thread->state_id; //set it to running state
			}
			
			if(setcontext(running_thread->context) == -1){
				return THREAD_FAILED;
			};
		}
		//printf("yielding to: %d\n", want_tid);
		return want_tid;
	}
}


void
thread_exit(int exit_code)
{
	if (ready_queue.head == NULL){
		exit_code = 0;
		exit(0); 
	} else {
		//printf("exiting: %d\n", (int)running_thread->thread_id);
		running_thread->state_id = quit;
		all_threads[running_thread->thread_id] = running_thread->state_id;
		thread_yield(THREAD_ANY);
	}
}

Tid
thread_kill(Tid tid)
{
	if (tid < 0 || tid > THREAD_MAX_THREADS-1 || tid == thread_id() || all_threads[tid] != ready){
		return THREAD_INVALID; 
	} 
	struct thread * curr = ready_queue.head; 
	while (curr->thread_id != tid){
		curr = curr->next;
	}
	curr->state_id = kill;
	all_threads[tid] = curr->state_id; 
	//printf("killed %d\n", (int) tid);
	return tid;
}

/**************************************************************************
 * Important: The rest of the code should be implemented in Assignment 2. *
 **************************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create()
{
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	TBD();

	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
	TBD();
	free(wq);
}

Tid
thread_sleep(struct wait_queue *queue)
{
	TBD();
	return THREAD_FAILED;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
	TBD();
	return 0;
}

/* suspend current thread until Thread tid exits */
Tid
thread_wait(Tid tid, int *exit_code)
{
	TBD();
	return 0;
}

struct lock {
	/* ... Fill this in ... */
};

struct lock *
lock_create()
{
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	TBD();

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

	TBD();

	free(lock);
}

void
lock_acquire(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

void
lock_release(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

struct cv {
	/* ... Fill this in ... */
};

struct cv *
cv_create()
{
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	TBD();

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

	TBD();

	free(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}
