#include <inttypes.h>
#include "../include/fsi_types_fwd.h"
#include "../include/fsi_indexer.h"
#include "../include/fsi_crawler.h"
#include "../include/fsi_worker.h"
#include "../include/fsi_utils.h"
#include "../include/fsi_queue.h"

#include <stddef.h>

#include "mos.h"

void fsi_idr_process_storage_queue(fsi_t_idr_indexer* indexer);

fsi_t_idr_indexer* fsi_create_indexer(fsi_t_idr_config* cfg) {
    if(cfg == NULL) {
        fsi_utils_report_error("indexer config is null. Cannot create crawler.");
        return NULL;
    }

    fsi_t_idr_indexer* indexer = malloc(sizeof(fsi_t_idr_indexer));
    if(indexer == NULL) {
        return NULL;
    }

    fsi_t_context* context = &indexer->context;

    indexer->context.crawler = fsi_cwl_create_crawler(cfg->crawler_cfg);
    indexer->context.worker = fsi_wrk_create_worker(cfg->worker_cfg);
    if(context->crawler == NULL || context->worker == NULL) {
        return NULL;
    }

    //TODO: change size to size of storage record struct
    context->storage_queue = fsi_create_queue(50000, sizeof(fsi_t_mos_file));
    if(context->storage_queue == NULL) {
        return NULL;
    }

    //connect queues
    //crawler owns file queue, worker reads from file queue
    context->worker->context.file_queue = context->crawler->context.file_queue;
    context->worker->context.storage_queue = context->storage_queue;

    mos_t_attr_info file_name_attr = {
        .name = "file_name",
        .type = MOS_ATTR_TYPE_STRING,
        .external_offset = offsetof(fsi_t_mos_file, file_name)
    };
    mos_t_attr_info file_path_attr = {
        .name = "file_path",
        .type = MOS_ATTR_TYPE_STRING,
        .external_offset = offsetof(fsi_t_mos_file, file_path)
    };

    mos_t_attr_info attributes[2];
    attributes[0] = file_name_attr;
    attributes[1] = file_path_attr;

    mos_t_idx file_name_idx = {
        .name = "idx_file_name",
        .type = MOS_IDX_HASH_MAP,
        .attribute = file_name_attr
    };

    mos_t_idx indexes[1];
    indexes[0] = file_name_idx;

    mos_t_config mos_cfg = {
        .max_records = cfg->records_count,
        .storage_path = cfg->storage_path,
        .attribute_count = 2,
        .index_count = 1,
        .attributes = attributes,
        .indexes = indexes,
    };

    indexer->context.storage = mos_create_storage(mos_cfg.storage_path, &mos_cfg);

    return indexer;
}

void fsi_idr_start(fsi_t_idr_indexer* indexer, const char** root_paths, size_t path_count) {
    //queues are created, crawler and worker are waiting for each other, so order of starting should not matter
    fsi_wrk_start_worker(indexer->context.worker);
    fsi_cwl_start_crawler(indexer->context.crawler, root_paths, path_count);
    fsi_idr_process_storage_queue(indexer);
}

void fsi_idr_process_storage_queue(fsi_t_idr_indexer* indexer) {
    fsi_t_mos_file file;
    uint64_t file_count = 0;
    while(fsi_queue_pop(indexer->context.storage_queue, &file)) {
        mos_storage_put(indexer->context.storage, file.file_id, &file);
        file_count++;

        if((file_count % 10000) == 0) {
            printf("[storage] Processed another 10000 files. %llu files processed\n", file_count);
        }
    }
}

void fsi_idr_stop(fsi_t_idr_indexer* indexer) {
    
}

void fsi_idr_add_path(char* path) {
    
}

void fsi_idr_remove_path(char* path) {
    
}