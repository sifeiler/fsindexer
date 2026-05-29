#include <stdint.h>

#include "../include/thr_thread.h"

//
// WINDOWS thread implementation
// The windows part was written with the help of AI.
//
#ifdef _WIN32
    #include <windows.h>
    #include <stdlib.h>

    thr_t_thread* thr_allocate_thread() {
        thr_t_thread *thread = (thr_t_thread*)malloc(sizeof(thr_t_thread));

        if (thread == NULL) {
            return NULL;
        }
        return thread;
    }

    int thr_thread_create(thr_t_thread* thread, void *func, void *func_args) {
        thread->thread = CreateThread(
            NULL,
            0,
            (LPTHREAD_START_ROUTINE)(func),
            func_args,
            0,
            NULL
        );
        return thread->thread ? 0 : -1;
    }

    void thr_thread_join(thr_t_thread* thread) {
        WaitForSingleObject(thread->thread, INFINITE);
        CloseHandle(thread->thread);
    }

    void thr_thread_destroy(thr_t_thread* thread) {
        CloseHandle(thread->thread);
        free(thread);
    }

    thr_t_mutex* thr_mtx_new() {
        CRITICAL_SECTION* mutex = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));

        if(mutex == NULL) {
            return NULL;
        }        
        InitializeCriticalSection(mutex);
        return mutex;
    }

    void thr_mtx_init(thr_t_mutex* mutex) {
        InitializeCriticalSection(mutex);
    }

    void thr_mtx_destroy(thr_t_mutex* mutex) {
        DeleteCriticalSection(mutex);
    }

    void thr_mtx_lock(thr_t_mutex* mutex) {
        EnterCriticalSection(mutex);
    }

    void thr_mtx_unlock(thr_t_mutex* mutex) {
        LeaveCriticalSection(mutex);
    }

    void thr_cond_init(thr_t_cond* cond) {
        InitializeConditionVariable(cond);
    }

    void thr_cond_wait(thr_t_cond* cond, thr_t_mutex* mutex) {
        SleepConditionVariableCS(cond, mutex, INFINITE);
    }

    void thr_cond_destroy(thr_t_cond* cond) {}

    //notify at least one thread
    void thr_cond_notify_one(thr_t_cond* cond) {
        WakeConditionVariable(cond);
    }

    void thr_cond_notify_all(thr_t_cond* cond) {
        WakeAllConditionVariable(cond);
    }

    uint64_t thr_get_id() {
        return (uint64_t)GetCurrentThreadId();
    }
    
//
// LINUX/POSIX thread implementation
//
#else
    #include <pthread.h>
    #include <stdlib.h>

    thr_t_thread* thr_allocate_thread() {
        thr_t_thread* thread = malloc(sizeof(thr_t_thread));

        if(thread == NULL) {
            return NULL;
        }
        return thread;
    }

    int thr_thread_create(thr_t_thread* thread, void *func, void *func_args) {
        return pthread_create(thread->thread,
                       NULL,
                       (void *(*)(void *))func,
                       func_args);
    }

    void thr_thread_join(thr_t_thread* thread) {
        pthread_join(*thread->thread, NULL);
    }

    void thr_thread_destroy(thr_t_thread* thread) {
        if(thread->thread != NULL) {
            free(thread->thread)
        }
        free(thread);
    }

    thr_t_mutex* thr_mtx_new() {
        pthread_mutex_t* mutex = malloc(sizeof(pthread_mutex_t));
        if(mutex == NULL) {
            return NULL;
        }
        pthread_mutex_init(mutex, NULL);
        return mutex;
    }

    void thr_mtx_init(thr_t_mutex* mutex) {
        pthread_mutex_init(mutex, NULL);
    }

    void thr_mtx_destroy(thr_t_mutex* mutex) {
        pthread_mutex_destroy(mutex);
        free(mutex);
    }

    void thr_mtx_lock(thr_t_mutex* mutex) {
        pthread_mutex_lock(mutex);
    }

    void thr_mtx_unlock(thr_t_mutex* mutex) {
        pthread_mutex_unlock(mutex);
    }

    void thr_cond_init(thr_t_cond* cond) {
        pthread_cond_init(cond);
    }

    void thr_cond_wait(thr_t_cond* cond, thr_t_mutex* mutex) {
        pthread_cond_wait(cond, mutex);
    }

    void thr_cond_destroy(thr_t_cond* cond) {
        pthread_cond_destroy(cond);
    }

    //notify at least one thread
    void thr_cond_notify_one(thr_t_cond* cond) {
        pthread_cond_signal(cond);
    }

    void thr_cond_notify_all(thr_t_cond* cond) {
        pthread_cond_broadcast(cond);
    }

    uint64_t thr_get_id() {
        return (uint64_t)pthread_self();
    }

#endif