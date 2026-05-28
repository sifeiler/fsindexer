#ifndef THR_THREAD_H
#define THR_THREAD_H

#include <stdint.h>

#ifdef _WIN32
    #include <windows.h>

    typedef struct thr_t_thread {
        HANDLE thread;
        //interrupted = 0 <-> no interrupt
        int interrupted;
    } thr_t_thread;
    typedef CRITICAL_SECTION thr_t_mutex;
    typedef CONDITION_VARIABLE thr_t_cond;
#else
    #include <pthread.h>

    typedef struct thr_t_thread {
        pthread_t* thread;
        int interrupted;
    } thr_t_thread;
    typedef pthread_mutex_t thr_t_mutex;
    typedef pthread_cond_t thr_t_cond;
#endif

typedef struct thr_t_thread_args {
    thr_t_thread* thread;
    void* context;
} thr_t_thread_args;

thr_t_thread* thr_allocate_thread();
int thr_thread_create(thr_t_thread* thread, void *func, void *func_args);
void thr_thread_join(thr_t_thread* thread);
thr_t_mutex* thr_mtx_new();
void thr_mtx_destroy(thr_t_mutex* mutex); 
void thr_mtx_init(thr_t_mutex* mutex); 
void thr_mtx_lock(thr_t_mutex* mutex);
void thr_mtx_unlock(thr_t_mutex* mutex);
void thr_cond_wait(thr_t_cond* cond, thr_t_mutex* mutex);
void thr_cond_init(thr_t_cond* cond);
void thr_cond_destroy(thr_t_cond* cond);
void thr_cond_notify_one(thr_t_cond* cond);
void thr_cond_notify_all(thr_t_cond* cond);
uint64_t thr_get_id();

#endif