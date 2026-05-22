#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stddef.h>

/* Minimal pthread-based thread pool.
   Each task is a void function with a userdata pointer.
   The pool waits for all submitted tasks to complete before returning
   from tp_wait().  No dynamic task queue — tasks are submitted in
   batches and the entire batch is waited on. */

typedef void (*TaskFn)(void *arg);

typedef struct {
    TaskFn fn;
    void *arg;
} Task;

typedef struct ThreadPool ThreadPool;

/* Create a pool with `n_threads` worker threads.
   n_threads == 0 creates a synchronous (no-thread) pool. */
ThreadPool *tp_create(int n_threads);
void tp_destroy(ThreadPool *tp);

/* Submit a batch of tasks.  The pool may execute them in parallel.
   `tasks` must remain valid until tp_wait() returns. */
void tp_submit_batch(ThreadPool *tp, Task *tasks, int n_tasks);

/* Block until all previously submitted tasks finish. */
void tp_wait(ThreadPool *tp);

/* Convenience: number of hardware threads (at least 1). */
int tp_hw_concurrency(void);

#endif
