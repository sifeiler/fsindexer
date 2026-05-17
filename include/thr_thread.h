#ifndef THR_THREAD_H
#define THR_THREAD_H

//
// WINDOWS thread implementation
// The windows part was written with the help of AI.
//
#ifdef _WIN32
    #include <windows.h>
    #include <stdlib.h>

    typedef HANDLE thr_t_thread;
    typedef CRITICAL_SECTION thr_t_mutex;
    typedef CONDITION_VARIABLE thr_t_cond;

    static inline thr_t_thread* thr_allocate_thread() {
        HANDLE *thread = (HANDLE*)malloc(sizeof(HANDLE));

        if (thread == NULL) {
            return NULL;
        }
        return thread;
    }

    static inline int thr_thread_create(thr_t_thread* thread, void *func, void *func_args) {
        *thread = CreateThread(
            NULL,
            0,
            (LPTHREAD_START_ROUTINE)(func),
            func_args,
            0,
            NULL
        );
        return *thread ? 0 : -1;
    }

    static inline void thr_thread_join(thr_t_thread* thread) {
        WaitForSingleObject(*thread, INFINITE);
        CloseHandle(*thread);
    }

    static inline thr_t_mutex* thr_mtx_new() {
        CRITICAL_SECTION* mutex = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));

        if(mutex == NULL) {
            return NULL;
        }        
        InitializeCriticalSection(mutex);
        return mutex;
    }

    static inline void thr_mtx_destroy(thr_t_mutex* mutex) {
        DeleteCriticalSection(mutex);
    }

    static inline void thr_mtx_lock(thr_t_mutex* mutex) {
        EnterCriticalSection(mutex);
    }

    static inline void thr_mtx_unlock(thr_t_mutex* mutex) {
        LeaveCriticalSection(mutex);
    }

    static inline void thr_cond_wait(thr_t_cond* cond, thr_t_mutex* mutex) {
        SleepConditionVariableCS(cond, mutex, INFINITE);
    }

    static inline void thr_cond_destroy(thr_t_cond* cond) {}

    //notify at least one thread
    static inline void thr_cond_notify_one(thr_t_cond* cond) {
        WakeConditionVariable(cond);
    }

    static inline void thr_cond_notify_all(thr_t_cond* cond) {
        WakeAllConditionVariable(cond);
    }
    
//
// LINUX/POSIX thread implementation
//
#else
    #include <pthread.h>
    #include <stdlib.h>
    
    typedef pthread_t thr_t_thread;
    typedef pthread_mutex_t thr_t_mutex;
    typedef pthread_cond_t thr_t_cond;

    static inline thr_t_thread* thr_allocate_thread() {
        pthread_t* thread = malloc(sizeof(pthread_t));

        if(thread == NULL) {
            return NULL;
        }
        return thread;
    }

    static inline int thr_thread_create(thr_t_thread* thread, void *func, void *func_args) {
        return pthread_create(thread,
                       NULL,
                       (void *(*)(void *))func,
                       func_args);
    }

    static inline void thr_thread_join(thr_t_thread* thread) {
        pthread_join(thread, NULL);
    }

    static inline thr_t_mutex* thr_mtx_new() {
        pthread_mutex_t* mutex = malloc(sizeof(pthread_mutex_t));
        if(mutex == NULL) {
            return NULL;
        }
        pthread_mutex_init(mutex, NULL);
        retrun mutex;
    }

    static inline void thr_mtx_destroy(thr_t_mutex* mutex) {
        pthread_mutex_destroy(m);
        free(m);
    }

    static inline void thr_mtx_lock(thr_t_mutex* mutex) {
        pthread_mutex_lock(mutex);
    }

    static inline void thr_mtx_unlock(thr_t_mutex* mutex) {
        pthread_mutex_unlock(mutex);
    }

    static inline void thr_cond_wait(thr_t_cond* cond, thr_t_mutex* mutex) {
        pthread_cond_wait(cond, mutex);
    }

    static inline void thr_cond_destroy(thr_t_cond* cond) {
        pthread_cond_destroy(cond);
    }

    //notify at least one thread
    static inline void thr_cond_notify_one(thr_t_cond* cond) {
        pthread_cond_signal(cond);
    }

    static inline void thr_cond_notify_all(thr_t_cond* cond) {
        pthread_cond_broadcast(cond);
    }

#endif
#endif