#ifndef PTHREAD_H
#define PTHREAD_H

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <process.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* clock_gettime for Windows - must be defined before pthread_cond_timedwait */
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

static inline int clock_gettime(int clk_id, struct timespec *tp) {
    if (clk_id == CLOCK_REALTIME) {
        FILETIME ft;
        ULARGE_INTEGER li;
        GetSystemTimeAsFileTime(&ft);
        li.LowPart = ft.dwLowDateTime;
        li.HighPart = ft.dwHighDateTime;
        li.QuadPart -= 116444736000000000ULL;
        tp->tv_sec = (long)(li.QuadPart / 10000000ULL);
        tp->tv_nsec = (long)((li.QuadPart % 10000000ULL) * 100);
    } else {
        /* CLOCK_MONOTONIC via QueryPerformanceCounter */
        static LARGE_INTEGER freq = {0};
        LARGE_INTEGER counter;
        if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&counter);
        long long ns = (counter.QuadPart * 1000000000LL) / freq.QuadPart;
        tp->tv_sec = (long)(ns / 1000000000LL);
        tp->tv_nsec = (long)(ns % 1000000000LL);
    }
    return 0;
}

/* pthread types */
typedef CRITICAL_SECTION pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;
typedef HANDLE pthread_t;

typedef struct {
    unsigned (__stdcall *start_routine)(void *);
    void *arg;
} pthread_create_args_t;

/* pthread_mutex */
static inline int pthread_mutex_init(pthread_mutex_t *mutex, const void *attr) {
    (void)attr;
    InitializeCriticalSection(mutex);
    return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t *mutex) {
    EnterCriticalSection(mutex);
    return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    LeaveCriticalSection(mutex);
    return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    DeleteCriticalSection(mutex);
    return 0;
}

/* pthread_cond */
static inline int pthread_cond_init(pthread_cond_t *cond, const void *attr) {
    (void)attr;
    InitializeConditionVariable(cond);
    return 0;
}

static inline int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    SleepConditionVariableCS(cond, mutex, INFINITE);
    return 0;
}

static inline int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    DWORD ms = (DWORD)((abstime->tv_sec - now.tv_sec) * 1000 +
                        (abstime->tv_nsec - now.tv_nsec) / 1000000);
    if (ms <= 0) ms = 1;
    if (SleepConditionVariableCS(cond, mutex, ms))
        return 0;
    return ETIMEDOUT;
}

static inline int pthread_cond_broadcast(pthread_cond_t *cond) {
    WakeAllConditionVariable(cond);
    return 0;
}

static inline int pthread_cond_signal(pthread_cond_t *cond) {
    WakeConditionVariable(cond);
    return 0;
}

static inline int pthread_cond_destroy(pthread_cond_t *cond) {
    (void)cond;
    return 0;
}

/* pthread_create / pthread_join */
static unsigned __stdcall pthread_create_wrapper(void *arg) {
    pthread_create_args_t *args = (pthread_create_args_t *)arg;
    args->start_routine(args->arg);
    free(args);
    return 0;
}

static inline int pthread_create(pthread_t *thread, const void *attr,
                                  void *(*start_routine)(void *), void *arg) {
    (void)attr;
    pthread_create_args_t *args = (pthread_create_args_t *)malloc(sizeof(pthread_create_args_t));
    if (!args) return ENOMEM;
    args->start_routine = (unsigned (__stdcall *)(void *))start_routine;
    args->arg = arg;
    *thread = (HANDLE)_beginthreadex(NULL, 0, pthread_create_wrapper, args, 0, NULL);
    if (*thread == 0) {
        free(args);
        return EAGAIN;
    }
    return 0;
}

static inline int pthread_join(pthread_t thread, void **retval) {
    (void)retval;
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
}

#ifdef __cplusplus
}
#endif

#else
#include_next <pthread.h>
#endif /* _WIN32 */

#endif /* PTHREAD_H */
