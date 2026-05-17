#include <inttypes.h>
#include "fsi_types_fwd.h"

typedef struct fsi_t_idr_config {
    //set parallelism = 1 if sequential processing is needed 
    uint64_t crawler_parallelism;
    uint64_t watcher_parallelism;
    uint64_t worker_parallelism;

    char* path_count;
    char* paths[];
} fsi_t_idr_config;

typedef struct fsi_t_idr_context {
    fsi_t_cwl_crawler* crawler;
    fsi_t_wat_watcher* watcher;
    fsi_t_wrk_worker* worker;

    char* path_count;
    char* paths[];
} fsi_t_context;

typedef struct fsi_t_idr_indexer {
    fsi_t_context context;
} fsi_t_idr_indexer;

fsi_t_idr_indexer* fsi_create_indexer(fsi_t_idr_config* cfg) {
    //create fsi_t_idr_indexer
}

void fsi_idr_start(fsi_t_idr_indexer* indexer) {

}

void fsi_idr_stop(fsi_t_idr_indexer* indexer) {
    
}

void fsi_idr_add_path(char* path) {
    
}

void fsi_idr_remove_path(char* path) {
    
}