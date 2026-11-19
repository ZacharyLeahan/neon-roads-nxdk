#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <threads.h>

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

static inline int clock_gettime(int clock_id, struct timespec *value)
{
    (void)clock_id;
    return timespec_get(value, TIME_UTC) == TIME_UTC ? 0 : -1;
}

static inline int posix_memalign(void **memory, size_t alignment, size_t size)
{
    *memory = aligned_alloc(alignment, size);
    return *memory == NULL ? -1 : 0;
}

#define unlink(path) remove(path)

typedef thrd_t pthread_t;
typedef mtx_t pthread_mutex_t;
typedef cnd_t pthread_cond_t;

typedef struct nxdk_pthread_start {
    void *(*function)(void *);
    void *argument;
} nxdk_pthread_start_t;

static inline int nxdk_pthread_trampoline(void *opaque)
{
    nxdk_pthread_start_t *start = (nxdk_pthread_start_t *)opaque;
    void *(*function)(void *) = start->function;
    void *argument = start->argument;
    free(start);
    (void)function(argument);
    return 0;
}

static inline int pthread_create(pthread_t *thread, const void *attributes,
                                 void *(*function)(void *), void *argument)
{
    nxdk_pthread_start_t *start;
    (void)attributes;
    start = malloc(sizeof(*start));
    if (start == NULL) {
        return -1;
    }
    start->function = function;
    start->argument = argument;
    if (thrd_create(thread, nxdk_pthread_trampoline, start) != thrd_success) {
        free(start);
        return -1;
    }
    return 0;
}

static inline int pthread_join(pthread_t thread, void **result)
{
    int thread_result = 0;
    int ret = thrd_join(thread, &thread_result);
    if (result != NULL) {
        *result = NULL;
    }
    return ret == thrd_success ? 0 : -1;
}

static inline int pthread_cancel(pthread_t thread)
{
    (void)thread;
    return -1;
}

static inline int pthread_mutex_init(pthread_mutex_t *mutex, const void *attributes)
{
    (void)attributes;
    return mtx_init(mutex, mtx_plain) == thrd_success ? 0 : -1;
}
static inline int pthread_mutex_destroy(pthread_mutex_t *mutex) { mtx_destroy(mutex); return 0; }
static inline int pthread_mutex_lock(pthread_mutex_t *mutex) { return mtx_lock(mutex) == thrd_success ? 0 : -1; }
static inline int pthread_mutex_unlock(pthread_mutex_t *mutex) { return mtx_unlock(mutex) == thrd_success ? 0 : -1; }
static inline int pthread_cond_init(pthread_cond_t *condition, const void *attributes) { (void)attributes; return cnd_init(condition) == thrd_success ? 0 : -1; }
static inline int pthread_cond_destroy(pthread_cond_t *condition) { cnd_destroy(condition); return 0; }
static inline int pthread_cond_broadcast(pthread_cond_t *condition) { return cnd_broadcast(condition) == thrd_success ? 0 : -1; }
static inline int pthread_cond_wait(pthread_cond_t *condition, pthread_mutex_t *mutex) { return cnd_wait(condition, mutex) == thrd_success ? 0 : -1; }
static inline int pthread_cond_timedwait(pthread_cond_t *condition, pthread_mutex_t *mutex, const struct timespec *timeout) { return cnd_timedwait(condition, mutex, timeout) == thrd_success ? 0 : -1; }
