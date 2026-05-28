#ifndef FSI_INDEXER_H
#define FSI_INDEXER_H

#include <inttypes.h>
#include "fsi_types_fwd.h"
#include <stddef.h>

#include "mos.h"

typedef struct fsi_t_idr_config {
    fsi_t_cwl_config* crawler_cfg;
    fsi_t_wrk_config* worker_cfg;
    uint64_t records_count;
    char* storage_path;
} fsi_t_idr_config;

typedef struct fsi_t_idr_context {
    fsi_t_cwl_crawler* crawler;
    fsi_t_wat_watcher* watcher;
    fsi_t_wrk_worker* worker;

    fsi_t_queue* storage_queue;
    mos_t_storage* storage;
} fsi_t_context;

typedef struct fsi_t_idr_indexer {
    fsi_t_context context;
} fsi_t_idr_indexer;

// Record bytes:
// file_id      8 bytes
// file_name   12 bytes for the string descriptor (actual string is stored in silo)
// file_path   12 bytes for the string descriptor (actual string is stored in silo)
//             --------
//             24 bytes in total
typedef struct fsi_t_mos_file {
    uint64_t file_id;
    mos_t_string file_name;
    mos_t_string file_path;
} fsi_t_mos_file;

fsi_t_idr_indexer* fsi_create_indexer(fsi_t_idr_config* cfg);
void fsi_idr_start(fsi_t_idr_indexer* indexer, const char** root_paths, size_t path_count);
void fsi_idr_stop(fsi_t_idr_indexer* indexer);
void fsi_idr_add_path(char* path);
void fsi_idr_remove_path(char* path);

#endif