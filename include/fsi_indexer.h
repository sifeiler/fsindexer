#ifndef FSI_INDEXER_H
#define FSI_INDEXER_H

#include <inttypes.h>
#include "fsi_types_fwd.h"
#include <stddef.h>

typedef struct fsi_t_idr_config {
    fsi_t_cwl_config* crawler_cfg;
    fsi_t_wrk_config* worker_cfg;
} fsi_t_idr_config;

typedef struct fsi_t_idr_context {
    fsi_t_cwl_crawler* crawler;
    fsi_t_wat_watcher* watcher;
    fsi_t_wrk_worker* worker;

    fsi_t_queue* storage_queue;
} fsi_t_context;

typedef struct fsi_t_idr_indexer {
    fsi_t_context context;
} fsi_t_idr_indexer;

// 28 bytes in storage:
// file_path   12 bytes for the string descriptor (actual string is stored in silo)
// created_at   8 bytes
// modified_at  8 bytes
//             --------
//             28 bytes
typedef struct {
    //mos_t_string file_path;
    uint64_t created_at;
    uint64_t modified_at;
} File;

fsi_t_idr_indexer* fsi_create_indexer(fsi_t_idr_config* cfg);
void fsi_idr_start(fsi_t_idr_indexer* indexer, const char** root_paths, size_t path_count);
void fsi_idr_stop(fsi_t_idr_indexer* indexer);
void fsi_idr_add_path(char* path);
void fsi_idr_remove_path(char* path);

#endif