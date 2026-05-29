#ifndef FSI_CRAWLER_H
#define FSI_CRAWLER_H

#include "fsi_types_fwd.h"
#include "thr_thread.h"
#include "fsi_os.h"
#include <inttypes.h>
#include <stddef.h>

typedef struct fsi_t_cwl_config {
    uint64_t parallelismn;
} fsi_t_cwl_config;

typedef struct fsi_t_cwl_context {
    fsi_t_queue* dir_queue;
    fsi_t_queue* file_queue;
    thr_t_thread* threads;
    thr_t_mutex lock;
    size_t thread_count;
    int threads_done_count;
    int threads_all_done;
} fsi_t_cwl_context;

typedef struct fsi_t_cwl_crawler {
    fsi_t_cwl_context context;
} fsi_t_cwl_crawler;

fsi_t_cwl_crawler* fsi_cwl_create_crawler(fsi_t_cwl_config* cfg);
int fsi_cwl_start_crawler(fsi_t_cwl_crawler* crawler, const char** root_paths, size_t path_count);
void fsi_cwl_stop_crawler(fsi_t_cwl_crawler* crawler);
void fsi_cwl_join_crawler(fsi_t_cwl_crawler* crawler);
void fsi_cwl_destroy_crawler(fsi_t_cwl_crawler* crawler);

#endif