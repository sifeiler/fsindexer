#include <inttypes.h>
#include "../include/fsi_types_fwd.h"
#include "../include/fsi_indexer.h"
#include "../include/fsi_crawler.h"
#include "../include/fsi_worker.h"
#include "../include/fsi_utils.h"
#include "../include/fsi_queue.h"

#include <stddef.h>

void fsi_idr_process_storage_queue(fsi_t_idr_indexer* indexer);

fsi_t_idr_indexer* fsi_create_indexer(fsi_t_idr_config* cfg) {
    if(cfg == NULL) {
        fsi_utils_report_error("indexer config is null. Cannot create crawler.");
        return;
    }

    fsi_t_idr_indexer* indexer = malloc(sizeof(fsi_t_idr_indexer));
    if(indexer == NULL) {
        return NULL;
    }

    fsi_t_context context = indexer->context;

    context.crawler = fsi_cwl_create_crawler(cfg->crawler_cfg);
    context.worker = fsi_wrk_create_worker(cfg->worker_cfg);
    if(context.crawler == NULL || context.worker == NULL) {
        return NULL;
    }

    //TODO: change size to size of storage record struct
    context.storage_queue = fsi_create_queue(50000, 10);
    if(context.storage_queue == NULL) {
        return NULL;
    }

    //connect queues
    fsi_t_cwl_context crawler_context = context.crawler->context;
    fsi_t_wrk_context worker_context = context.worker->context;

    //crawler owns file queue, worker reads from file queue
    worker_context.file_queue = crawler_context.file_queue;
    worker_context.storage_queue = context.storage_queue;

    return indexer;
}

void fsi_idr_start(fsi_t_idr_indexer* indexer, const char** root_paths, size_t path_count) {
    //queues are created, crawler and worker are waiting for each other, so order of starting should not matter
    fsi_wrk_start_worker(indexer->context.worker);
    fsi_cwl_start_crawler(indexer->context.crawler, root_paths, path_count);
    fsi_idr_process_storage_queue(indexer);
}

void fsi_idr_process_storage_queue(fsi_t_idr_indexer* indexer) {
    //TODO: read from queue and put to mos storage (single threaded!!!)
}

void fsi_idr_stop(fsi_t_idr_indexer* indexer) {
    
}

void fsi_idr_add_path(char* path) {
    
}

void fsi_idr_remove_path(char* path) {
    
}