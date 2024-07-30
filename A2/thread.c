#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdio.h>
#include "thread.h"
#include <string.h>
#include "malloc369.h"
#include "interrupt.h"
// #define DEBUG_USE_VALGRIND

// #ifdef DEBUG_USE_VALGRIND
// #include <valgrind.h>
// #endif

/* This is the wait queue structure, needed for Assignment 2. */
struct wait_queue {
    /* ... Fill this in Assignment 2 ... */
    struct thread * head;
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
    struct wait_queue *waitqueue;
    int exitcode;
};
struct queue {
      struct thread * head;
};

typedef enum
{
  empty = 0,
  ready = 1,
  running = 2,
  killed = 3,
  quit = 4,
  sleeping = 5
} state;

int all_threads[THREAD_MAX_THREADS];
int exitcodes[THREAD_MAX_THREADS];
struct thread **threads; //sad, don't want to change the all_threads I've already implemented... wrong design choice!!!
struct queue ready_queue;
struct thread * running_thread;
/**************************************************************************
 * Assignment 1: Refer to thread.h for the detailed descriptions of the six
 *               functions you need to implement.
 **************************************************************************/

void print_queue(void* q){
    struct queue *queue = (struct queue *) q;
    unintr_printf("-------------\n");
    if (!queue->head){
        unintr_printf("Nothing in queue\n");
    } else {
        struct thread * curr = queue->head;
        while (curr != NULL){
            unintr_printf("[id: %d, state: %d] -> ", (int)curr->thread_id, (int)curr->state_id);
            curr = curr->next;
        }
        unintr_printf("queue end.\n");
    }
}


void enqueue(void* q, struct thread * new_thread){
    struct queue *q_ptr = (struct queue *) q;
    if (!q_ptr->head){
        q_ptr->head = new_thread;
    } else {
        struct thread * curr = q_ptr->head;
        while (curr->next != NULL){
            curr = curr->next;
        }
        curr->next = new_thread;
    }
}

struct thread *dequeue(void* q, Tid thread_id){
    struct queue *q_ptr = (struct queue *) q;
  if (!q_ptr->head){
    return NULL;
  } else if (q_ptr->head->thread_id == thread_id){
    struct thread * target = q_ptr->head;
    q_ptr->head = target->next;
    target->next = NULL;
    return target;
  } else {
    struct thread * prev = q_ptr->head;
    struct thread * curr = prev->next;
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
    int enabled = interrupts_set(false);
    threads = malloc369(sizeof(void *) * THREAD_MAX_THREADS);
    for (int i = 0; i < THREAD_MAX_THREADS; i++){
        all_threads[i] = 0;
        threads[i] = NULL;
        exitcodes[i] = 0;
    }
    ready_queue.head = NULL;
    // next we define the running thread.
    running_thread = (struct thread *) malloc369(sizeof(struct thread));
    running_thread->thread_id = 0;
    running_thread->state_id = running;
    running_thread->context = (ucontext_t *) malloc369(sizeof(ucontext_t));
    running_thread->next = NULL;
    running_thread->waitqueue = wait_queue_create();
    running_thread->exitcode = 0;
    all_threads[0] = running_thread->state_id;
    threads[0] = running_thread;
    
    interrupts_set(enabled);
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
    if (!interrupts_enabled()){
        interrupts_on();
    }
    thread_main(arg); // call thread_main() function with arg
    thread_exit(0);
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
    int enabled = interrupts_set(false);
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
    struct thread * new_thread = (struct thread*)malloc369(sizeof(struct thread));
    if (new_thread == NULL){
        return THREAD_NOMEMORY;
    }
    new_thread->thread_id = id;
    new_thread->state_id = ready;
    new_thread->context = (ucontext_t *) malloc369(sizeof(ucontext_t));
    new_thread->next = NULL;
    new_thread->waitqueue = wait_queue_create();
    new_thread->exitcode = 0;
    all_threads[id] = new_thread->state_id;
    threads[id] = new_thread;
    int interrupt = interrupts_enabled();
    if(getcontext(new_thread->context) == -1){
        return THREAD_FAILED;
    }
    interrupts_set(interrupt);
    new_thread->context->uc_stack.ss_sp = (void *) malloc369(THREAD_MIN_STACK);
    if (!new_thread->context->uc_stack.ss_sp){
        free369(new_thread);
        return THREAD_NOMEMORY;
    }
    new_thread->context->uc_stack.ss_size = THREAD_MIN_STACK;
    new_thread->context->uc_mcontext.gregs[REG_RSP] = (unsigned long)
    new_thread->context->uc_stack.ss_sp + new_thread->context->uc_stack.ss_size - sizeof(long);
    new_thread->context->uc_mcontext.gregs[REG_RIP] = (unsigned long) &thread_stub;
    new_thread->context->uc_mcontext.gregs[REG_RDI] = (unsigned long) fn;
    new_thread->context->uc_mcontext.gregs[REG_RSI] = (unsigned long) parg;
    enqueue(&ready_queue, new_thread);
    interrupts_set(enabled);
    // print_queue(&ready_queue);
    return id;
}

Tid
thread_yield(Tid want_tid)
{
    int enabled = interrupts_set(false);
    int contextSet = 0; // check if set context has run once already
    // printf("before zombie\n");
    //unintr_printf("calling yield: %d\n", running_thread->thread_id);
    for (int i = 0; i < THREAD_MAX_THREADS; i++){
        if (all_threads[i] == quit && i != thread_id()){
            //unintr_printf("freeing i: %d\n", i);
            struct thread * curr = dequeue(&ready_queue, i);
            if(!curr){
                continue;
            }
            free369(curr->context->uc_stack.ss_sp);
            free369(curr->context);
            wait_queue_destroy(curr->waitqueue);
            free369(curr);
            all_threads[i] = empty;
            threads[i] = NULL;
        }
    }
    //print_queue(&ready_queue);
    
    Tid id;
    //unintr_printf("thread. want_tid: %d\n", (int) want_tid);
    if (want_tid == THREAD_SELF || want_tid == thread_id()){
        //printf("%d\n", (int)want_tid);
        interrupts_set(enabled);
        return thread_id(); //yield to self
    } else if (want_tid == THREAD_ANY){ // yield to next available thread
        if (!ready_queue.head || ready_queue.head->state_id == sleeping){
            interrupts_set(enabled);
            //unintr_printf("none\n");
            return THREAD_NONE;
        }
        if (running_thread->state_id == killed){
            exitcodes[running_thread->thread_id] = -SIGKILL;
            interrupts_set(enabled);
            thread_exit(-SIGKILL);
        }
        id = ready_queue.head->thread_id;
        //unintr_printf("yield, any, id: %d", id);
        //printf("ok to proceed\n");
        int interrupt = interrupts_enabled();
        if(getcontext(running_thread->context) == -1){
            return THREAD_FAILED;
        }
        
        if (!contextSet){
            //printf("setting context to 1\n");
            contextSet = 1;
            Tid running_id = thread_id();
            if (all_threads[running_id] != quit && all_threads[running_id] != killed){
                running_thread->state_id = ready;
                all_threads[running_id] = running_thread->state_id;
            }
            enqueue(&ready_queue, running_thread);
            // yield to thread that has corresponding thread_id
            running_thread = dequeue(&ready_queue, id);
            if(!running_thread){
                interrupts_set(enabled);
                return THREAD_NONE;
            }

            if (all_threads[id] != quit && all_threads[id] != killed){
                running_thread->state_id = running;
                all_threads[id] = running_thread->state_id; //set it to running state
            }
            interrupts_set(interrupt);
            if(setcontext(running_thread->context) == -1){
                return THREAD_FAILED;
            };
        }
        interrupts_set(enabled);
        //Tid now = thread_id();
        //printf("yielding to: %d\n", (int)now);
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
        if (running_thread->state_id == killed){
            exitcodes[running_thread->thread_id] = -SIGKILL;
            interrupts_set(enabled);
            thread_exit(-SIGKILL);
        }
        int interrupt = interrupts_enabled();
        if(getcontext(running_thread->context) == -1){
            return THREAD_FAILED;
        }

        if (!contextSet){
            contextSet = 1;
            Tid running_id = thread_id();
            if (all_threads[running_id] != quit && all_threads[running_id] != killed){
                running_thread->state_id = ready;
                all_threads[running_id] = running_thread->state_id;
            }
            enqueue(&ready_queue, running_thread);
            //printf("adding %d to ready queue\n", (int) running_thread->thread_id);
            running_thread = dequeue(&ready_queue, id);
            if (!running_thread){
                interrupts_set(enabled);
                return THREAD_NONE;
            }
            if (all_threads[id] != quit && all_threads[id] != killed){
                running_thread->state_id = running;
                //printf("in if, now running\n");
                all_threads[id] = running_thread->state_id; //set it to running state
            }
            interrupts_set(interrupt);
            if(setcontext(running_thread->context) == -1){
                return THREAD_FAILED;
            };
        }
        interrupts_set(enabled);
        //printf("yielding to: %d\n", want_tid);
        return want_tid;
    }
}


void
thread_exit(int exit_code)
{
    int enabled = interrupts_set(false);
    if(running_thread->waitqueue){
        thread_wakeup(running_thread->waitqueue, 1);
    }
    //unintr_printf("thread %d exiting, exitcode given: %d\n", (int) running_thread->thread_id, exit_code);
    //print_queue(&ready_queue);
    if (!ready_queue.head){
        //unintr_printf("no ready, exit\n");
        if(running_thread->waitqueue){
            wait_queue_destroy(running_thread->waitqueue);
        }
        free369(threads);
        interrupts_set(enabled);
        exit(exit_code);
    } else {
        //printf("exiting: %d\n", (int)running_thread->thread_id);
        running_thread->state_id = quit;
        all_threads[running_thread->thread_id] = running_thread->state_id;
        running_thread->exitcode = exit_code;
        exitcodes[running_thread->thread_id] = exit_code;
        thread_wakeup(running_thread->waitqueue, 1);
        //unintr_printf("after wakeup\n");
        thread_yield(THREAD_ANY);
        interrupts_set(enabled);
        exit(0);
    }
}

Tid
thread_kill(Tid tid)
{
    int enabled = interrupts_set(false);
    //unintr_printf("running %d, killing %d\n",running_thread->thread_id, tid);
    if (tid < 0 || tid > THREAD_MAX_THREADS-1 || tid == thread_id() || (all_threads[tid] != ready && all_threads[tid] != sleeping)){
        //unintr_printf("invalid\n");
        interrupts_set(enabled);
        return THREAD_INVALID;
    }
    struct thread * curr = threads[tid];
    assert(curr);
    curr->state_id = killed;
    all_threads[tid] = curr->state_id;
    curr->exitcode = -SIGKILL;
    exitcodes[tid] = -SIGKILL;
    interrupts_set(enabled);
    //printf("killed %d\n", (int) tid);
    return tid;
}

/**************************************************************************
 * Important: The rest of the code should be implemented in Assignment 2. *
 **************************************************************************/

/* make sure to fill the wait_queue structure defined above */
void wait_enqueue(struct wait_queue *queue, struct thread * new_thread){
    int enabled = interrupts_set(false);
    if (!queue->head){
        queue->head = new_thread;
    } else {
        struct thread * curr = queue->head;
        while (curr->next != NULL){
            curr = curr->next;
        }
        curr->next = new_thread;
    }
    interrupts_set(enabled);
}

struct thread *wait_dequeue(struct wait_queue *queue, Tid thread_id){
    int enabled = interrupts_set(false);
    if (!queue->head){
        interrupts_set(enabled);
        return NULL;
    } else if (queue->head->thread_id == thread_id){
        struct thread * target = queue->head;
        queue->head = target->next;
        target->next = NULL;
        interrupts_set(enabled);
        return target;
    } else {
        struct thread * prev = queue->head;
        struct thread * curr = prev->next;
        while (curr->thread_id != thread_id){
        prev = prev->next;
        curr = curr->next;
        }
        if (curr) {
            prev->next = curr->next;
            curr->next = NULL;
            return curr;
        }
        interrupts_set(enabled);
        return NULL;
    }
}

struct wait_queue *
wait_queue_create()
{
    struct wait_queue *wq;

    wq = malloc369(sizeof(struct wait_queue));
    assert(wq);
    wq->head = NULL;
    return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
    assert(!wq->head);
    free369(wq);
}

Tid
thread_sleep(struct wait_queue *queue)
{
    int contextSet = 0;
    int enabled = interrupts_set(false);
    if (!queue){
        interrupts_set(enabled);
        return THREAD_INVALID;
    }
    if (!ready_queue.head){
        //unintr_printf("no ready\n");
        interrupts_set(enabled);
        return THREAD_NONE;
    }
    struct thread* next = dequeue(&ready_queue, ready_queue.head->thread_id);
    Tid id = next->thread_id;
    //unintr_printf("next id: %d\n", (int)id);
    if (running_thread->state_id == killed){
        interrupts_set(enabled);
        thread_exit(-SIGKILL);
    }
    int interrupt = interrupts_enabled();
    if(getcontext(running_thread->context) == -1){
        return THREAD_FAILED;
    }
    if (!contextSet){
        //printf("setting context to 1\n");
        contextSet = 1;
        Tid running_id = thread_id();
        if (all_threads[running_id] != quit && all_threads[running_id] != killed){
            running_thread->state_id = sleeping;
            all_threads[running_id] = running_thread->state_id;
        }
        wait_enqueue(queue, running_thread);
        //printf("adding %d to ready queue\n", (int) running_thread->thread_id);
        //printf("dequed thread: %d\n", (int) thread_id());
        if (all_threads[id] != quit && all_threads[id] != killed){
            running_thread = next;
            running_thread->state_id = running;
            all_threads[id] = running_thread->state_id; //set it to running state
        }
        interrupts_set(interrupt);
        if(setcontext(running_thread->context) == -1){
            return THREAD_FAILED;
        };
    }
    interrupts_set(enabled);
    return next->thread_id;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
    bool enabled = interrupts_set(false);
    if (!queue){
        interrupts_set(enabled);
        return 0;
    }
    int count = 0;
    if (all == 1){
        while (queue->head){
            struct thread* temp = wait_dequeue(queue, queue->head->thread_id);
            temp->state_id = ready;
            all_threads[temp->thread_id] = ready;
            enqueue(&ready_queue, temp);
            count ++;
        }
    } else {
        if(!queue->head){
            interrupts_set(enabled);
            return 0;
        }
        struct thread* temp = wait_dequeue(queue, queue->head->thread_id);
        temp->state_id = ready;
        all_threads[temp->thread_id] = ready;
        enqueue(&ready_queue, temp);
        count ++;
    }
    interrupts_set(enabled);
    return count;
}

/* suspend current thread until Thread tid exits */
Tid
thread_wait(Tid tid, int *exit_code)
{
    int enabled = interrupts_set(0);
    //unintr_printf("running thread: %d, tid: %d\n", (int)running_thread->thread_id, (int)tid);
    if (tid < 0 || tid >= THREAD_MAX_THREADS || tid == running_thread->thread_id || all_threads[tid] == empty || all_threads[tid] == killed || all_threads[tid] == quit){
        // unintr_printf("invalid\n");
        // unintr_printf("tid state: %d", all_threads[tid]);
        if (exitcodes[tid] != 0){
            if (exit_code){
                *exit_code = exitcodes[tid];
            }
            if (exitcodes[tid] == -SIGKILL){
                //unintr_printf("waiting on killed\n");
                interrupts_set(enabled);
                return THREAD_INVALID;
            }
            interrupts_set(enabled);
            return tid;
        }
        interrupts_set(enabled);
        return THREAD_INVALID;
    }
    struct thread *tid_thread = threads[tid];
    assert(tid_thread);
    if(tid_thread->waitqueue->head){ //ensure only 1 thread in wq
        return THREAD_INVALID;
    }
    thread_sleep(tid_thread->waitqueue);
    if (exit_code){
        *exit_code = exitcodes[tid];
    }
    interrupts_set(enabled);
    return tid;
}

struct lock {
    /* ... Fill this in ... */
    Tid hold_id;
    struct wait_queue* wait_queue;
};

struct lock *
lock_create()
{
    int enabled = interrupts_set(false);
    struct lock *lock;

    lock = malloc369(sizeof(struct lock));
    assert(lock);

    lock->hold_id = THREAD_INVALID;
    lock->wait_queue = wait_queue_create();
    interrupts_set(enabled);
    return lock;
}

void
lock_destroy(struct lock *lock)
{
    int enabled = interrupts_set(false);
    assert(lock != NULL);

    if (lock->hold_id == THREAD_INVALID && !lock->wait_queue->head){
        free369(lock);
    }
    interrupts_set(enabled);
}

void
lock_acquire(struct lock *lock)
{
    int enabled = interrupts_set(false);
    assert(lock != NULL);
    while (lock->hold_id != THREAD_INVALID){
        thread_sleep(lock->wait_queue);
    }
    lock->hold_id = thread_id();
    interrupts_set(enabled);
}

void
lock_release(struct lock *lock)
{
    int enabled = interrupts_set(false);
    assert(lock != NULL);
    if (running_thread->thread_id == lock->hold_id){
        lock->hold_id = THREAD_INVALID;
        thread_wakeup(lock->wait_queue, 1);
    }
    interrupts_set(enabled);
}

struct cv {
    /* ... Fill this in ... */
    struct wait_queue* wait_queue;
};

struct cv *
cv_create()
{
    int enabled = interrupts_set(false);
    struct cv *cv;

    cv = malloc(sizeof(struct cv));
    assert(cv);
    cv->wait_queue = wait_queue_create();
    interrupts_set(enabled);
    return cv;
}

void
cv_destroy(struct cv *cv)
{
    int enabled = interrupts_set(false);
    assert(cv != NULL);

    if (!cv->wait_queue->head){
        wait_queue_destroy(cv->wait_queue);
    }
    free369(cv);
    interrupts_set(enabled);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
    int enabled = interrupts_set(false);
    assert(cv != NULL);
    assert(lock != NULL);
    if(lock->hold_id == running_thread->thread_id){
        lock_release(lock);
        thread_sleep(cv->wait_queue);
        interrupts_set(enabled);
        lock_acquire(lock);
    }
    interrupts_set(enabled);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
    int enabled = interrupts_set(false);
    assert(cv != NULL);
    assert(lock != NULL);
    if (lock->hold_id == running_thread->thread_id){
        thread_wakeup(cv->wait_queue, 0);
    }
    interrupts_set(enabled);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
    int enabled = interrupts_set(false);
    assert(cv != NULL);
    assert(lock != NULL);
    if (lock->hold_id == running_thread->thread_id){
        thread_wakeup(cv->wait_queue, 1);
    }
    interrupts_set(enabled);
}
