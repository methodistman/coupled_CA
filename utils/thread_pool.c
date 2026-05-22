#include "thread_pool.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

struct ThreadPool {
    pthread_t *threads;
    int n_threads;
    int active;

    /* Current batch */
    Task *tasks;
    int n_tasks;
    int next_task;
    pthread_mutex_t lock;
    pthread_cond_t  work_avail;
    pthread_cond_t  all_done;
    int tasks_running;
};

static void *tp_worker(void *arg) {
    ThreadPool *tp = arg;
    while (1) {
        pthread_mutex_lock(&tp->lock);
        while (tp->active && tp->next_task >= tp->n_tasks && tp->tasks_running == 0) {
            pthread_cond_wait(&tp->work_avail, &tp->lock);
        }
        if (!tp->active) {
            pthread_mutex_unlock(&tp->lock);
            break;
        }
        if (tp->next_task < tp->n_tasks) {
            Task t = tp->tasks[tp->next_task++];
            tp->tasks_running++;
            pthread_mutex_unlock(&tp->lock);
            t.fn(t.arg);
            pthread_mutex_lock(&tp->lock);
            tp->tasks_running--;
            if (tp->next_task >= tp->n_tasks && tp->tasks_running == 0)
                pthread_cond_broadcast(&tp->all_done);
            pthread_mutex_unlock(&tp->lock);
        } else {
            pthread_mutex_unlock(&tp->lock);
        }
    }
    return NULL;
}

ThreadPool *tp_create(int n_threads) {
    if (n_threads < 1) n_threads = 1;
    ThreadPool *tp = calloc(1, sizeof(ThreadPool));
    if (!tp) return NULL;
    tp->n_threads = n_threads;
    tp->active = 1;
    pthread_mutex_init(&tp->lock, NULL);
    pthread_cond_init(&tp->work_avail, NULL);
    pthread_cond_init(&tp->all_done, NULL);
    tp->threads = calloc(n_threads, sizeof(pthread_t));
    if (!tp->threads) { free(tp); return NULL; }
    for (int i = 0; i < n_threads; i++) {
        pthread_create(&tp->threads[i], NULL, tp_worker, tp);
    }
    return tp;
}

void tp_destroy(ThreadPool *tp) {
    if (!tp) return;
    pthread_mutex_lock(&tp->lock);
    tp->active = 0;
    pthread_cond_broadcast(&tp->work_avail);
    pthread_mutex_unlock(&tp->lock);
    for (int i = 0; i < tp->n_threads; i++)
        pthread_join(tp->threads[i], NULL);
    pthread_mutex_destroy(&tp->lock);
    pthread_cond_destroy(&tp->work_avail);
    pthread_cond_destroy(&tp->all_done);
    free(tp->threads);
    free(tp);
}

void tp_submit_batch(ThreadPool *tp, Task *tasks, int n_tasks) {
    if (!tp || n_tasks <= 0) return;
    pthread_mutex_lock(&tp->lock);
    tp->tasks = tasks;
    tp->n_tasks = n_tasks;
    tp->next_task = 0;
    tp->tasks_running = 0;
    pthread_cond_broadcast(&tp->work_avail);
    pthread_mutex_unlock(&tp->lock);
}

void tp_wait(ThreadPool *tp) {
    if (!tp) return;
    pthread_mutex_lock(&tp->lock);
    while (tp->next_task < tp->n_tasks || tp->tasks_running > 0) {
        pthread_cond_wait(&tp->all_done, &tp->lock);
    }
    pthread_mutex_unlock(&tp->lock);
}

int tp_hw_concurrency(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? (int)n : 1;
}
