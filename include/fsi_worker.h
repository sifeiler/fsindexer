#ifndef FSI_WORKER_H
#define FSI_WORKER_H

#include "fsi_types_fwd.h"
#include "thr_thread.h"
#include "fsi_os.h"
#include <inttypes.h>
#include "fsi_queue.h"
#include <stddef.h>

typedef struct fsi_t_wrk_config {
    uint64_t parallelismn;
} fsi_t_wrk_config;

typedef struct fsi_t_wrk_context {
    fsi_t_queue* file_queue;
    fsi_t_queue* storage_queue;
    thr_t_thread* threads;
    thr_t_mutex lock;
    size_t thread_count;
    int threads_done_count;
    int threads_all_done;
} fsi_t_wrk_context;

typedef struct fsi_t_wrk_worker {
    fsi_t_wrk_context context;
} fsi_t_wrk_worker;

fsi_t_wrk_worker* fsi_wrk_create_worker(fsi_t_wrk_config* cfg);
int fsi_wrk_start_worker(fsi_t_wrk_worker* worker);
void fsi_wrk_stop_worker(fsi_t_wrk_worker* worker);
void fsi_wrk_join_worker(fsi_t_wrk_worker* worker);
void fsi_wrk_destroy_worker(fsi_t_wrk_worker* worker);

#endif