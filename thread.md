# Multithreading
[MIT6.1810 Multithreading Page](https://pdos.csail.mit.edu/6.S081/2022/labs/thread.html)

This lab focuses on the implementation of user-level threads (ULTs) in xv6. 

## Uthread: switching between threads
This task is a simple exercise to get familiar with the **Creation** and **Schedule** of threads. We can just mimic the logic of kernel threads.

First, include necessary headers in `uthread.c`
```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/spinlock.h"
#include "kernel/riscv.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/proc.h"
```
Be careful about the order of headers. The order of headers matters because of the dependencies between them.

Add a context in thread structure:
```c
struct thread {
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
  // * Add a context to be stored and restored between switch
  struct context context;
};
```

When creating a thread, set the function it will execute to `ra` and git it a new stack of `STACK_SIZE`:
```c
void 
thread_create(void (*func)())
{
  ...
  // YOUR CODE HERE
  // * return address set to func, when sched, it will be executed
  t->context.ra = (uint64)func;
  // * a new stack is created, sp is set to the top of the stack
  t->context.sp = (uint64)t->stack + STACK_SIZE - 1;
}
```
`uthread_swtch.S` is the same as `swtch.S` in the kernel, we skip here.

In `thread_schedule`, invoke context switch:
```c
void 
thread_schedule(void)
{
  ...
  if (current_thread != next_thread) {         /* switch threads?  */
    ...
    /* YOUR CODE HERE
     * Invoke thread_switch to switch from t to next_thread:
     * thread_switch(??, ??);
     */
    thread_switch((uint64)&t->context,(uint64)&current_thread->context);
  } else
    next_thread = 0;
}
```

## Using threads
This task is to implement a simple thread-safe put-get hashtable. 
The key is to use locks to protect the shared data.

First, we define locks for each bucket:
```c
// * Add locks
pthread_mutex_t lock[NBUCKET]; // a lock per bucket
```

Then in main, we initialize these locks:
```c
int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  double t1, t0;
  // * initialize the locks
  for(int i = 0; i < NBUCKET; i++)
    pthread_mutex_init(&lock[i], NULL);
  ...
}
```

Finally, in `put`, when we want to insert a key, we should `acquire` the lock:
```c
static 
void put(int key, int value)
{
  ...
  if(e){
    // update the existing key.
    e->value = value;
  } else {
    // the new is new.
    // * Insert bewteen lock
    pthread_mutex_lock(&lock[i]);       // acquire lock
    insert(key, value, &table[i], table[i]);
    pthread_mutex_unlock(&lock[i]);     // release lock
  }
}
```

## Barrier
This task is to implement a barrier that allows a set of threads to synchronize their execution. 

The key is to use a condition variable and a counter. When a thread reaches the barrier, it should wait until all threads have reached the barrier. 

The barrier structure is given, we only need to implement the barrier function:
```c
static void 
barrier()
{
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  //
  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread += 1;
  if(bstate.nthread >= nthread){
    bstate.nthread = 0;
    bstate.round += 1;
    pthread_mutex_unlock(&bstate.barrier_mutex);
    pthread_cond_broadcast(&bstate.barrier_cond);     // wake up every thread sleeping on cond
  } else {
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
    pthread_mutex_unlock(&bstate.barrier_mutex); // when be waken up,hold the lock,should be released
  } 
}
```
## Test
run `make grade` to test your code. Don't forget to add a `time.txt` to get a full score.