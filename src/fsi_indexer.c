#include <inttypes.h>
#include "../include/fsi_types_fwd.h"
#include "../include/fsi_indexer.h"
#include "../include/fsi_crawler.h"
#include "../include/fsi_worker.h"
#include "../include/fsi_utils.h"
#include "../include/fsi_queue.h"

#include <stddef.h>

#include "mos.h"
#include "mos_qry.h"

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
    context->storage_queue = fsi_queue_create(50000, sizeof(fsi_t_mos_file));
    if(context->storage_queue == NULL) {
        return NULL;
    }

    //connect queues
    //crawler owns file queue, worker reads from file queue
    context->worker->context.file_queue = context->crawler->context.file_queue;
    context->worker->context.storage_queue = context->storage_queue;

    mos_t_attr_info file_id_attr = {
        .name = "file_id",
        .type = MOS_ATTR_TYPE_UINT64,
        .byte_size = 8,
        .external_offset = offsetof(fsi_t_mos_file, file_id)
    };
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

    mos_t_attr_info attributes[3];
    attributes[0] = file_id_attr;
    attributes[1] = file_name_attr;
    attributes[2] = file_path_attr;
    
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
        .attribute_count = 3,
        .index_count = 1,
        .attributes = attributes,
        .indexes = indexes
    };

    mos_t_storage* storage = mos_create_storage(mos_cfg.storage_path, &mos_cfg);
    if(storage == NULL) {
        printf("Failed to create indexer. Cannot allocate storage.");
        return NULL;
    }

    indexer->context.storage = storage;
    return indexer;
}

void fsi_idr_start(fsi_t_idr_indexer* indexer, const char** root_paths, size_t path_count) {
    //queues are created, crawler and worker are waiting for each other, so order of starting should not matter
    fsi_wrk_start_worker(indexer->context.worker);
    fsi_cwl_start_crawler(indexer->context.crawler, root_paths, path_count);
    fsi_idr_process_storage_queue(indexer);
}

void fsi_idr_process_storage_queue(fsi_t_idr_indexer* indexer) {
    printf("Baseline indexing: start\n");
    fsi_t_mos_file file;
    uint64_t file_count = 0;
    while(fsi_queue_pop(indexer->context.storage_queue, &file) == FSI_QUEUE_OK) {
        mos_storage_put(indexer->context.storage, file.file_id, &file);
        file_count++;

        if((file_count % 10000) == 0) {
            printf("[storage] Processed another 10000 files. %llu files processed\n", file_count);
        }
    }
    //wait for all threads to finish
    fsi_cwl_join_crawler(indexer->context.crawler);
    fsi_wrk_join_worker(indexer->context.worker);

    printf("Baseline indexing: done\n");
    printf("--------------------------------------------------------\n");

    printf("Give me a filename and I will tell you its path:\n");
    char filename[FSI_PATH_MAX] = {0};

    char fmt[16];
    snprintf(fmt, sizeof(fmt), "%%%ds", FSI_PATH_MAX - 1);
    while (scanf(fmt, filename) == 1)
    {
        int filename_len = strlen(filename);
        mos_t_qry_search_step filename_step = 
        { 
            .op = MOS_QRY_OP_EQ,
            .attribute_query = 
            { 
                .attribute_name = "file_name", 
                .value = 
                { 
                    .type = MOS_ATTR_TYPE_STRING, 
                    .char_val = filename, 
                    .byte_length = filename_len
                }
            } 
        };

        mos_t_qry query = {
            .query = &filename_step
        };
    
        const mos_t_qry_bmp* result = mos_storage_search(indexer->context.storage, &query);
        uint64_t expected_ones = mos_qry_bmp_count_ones(result);

        int64_t* row_ids_buffer = malloc(expected_ones * sizeof(int64_t));
        if (!row_ids_buffer) {
            printf("Cannot allocate row_ids_buffer.\n");
            continue;
        }

        uint64_t actual_ones = mos_qry_bmp_get_row_ids(result, row_ids_buffer);
        if(expected_ones != actual_ones) {
            printf("Result bitmap missmatch. Expected %" PRIu64 ", was %" PRIu64 "\n", expected_ones, actual_ones);
            continue;
        }

        for (size_t i = 0; i < actual_ones; i++) {
            int64_t row_id = row_ids_buffer[i];
            fsi_t_mos_file* file = (fsi_t_mos_file*)mos_storage_get_data_for_row_id(indexer->context.storage, row_id);
            printf("%.*s\n", file->file_path.str_len, file->file_path.str);
        }
        free(row_ids_buffer);
        memset(filename, 0, FSI_PATH_MAX);
    }
}

void fsi_idr_stop(fsi_t_idr_indexer* indexer) {
    
}

void fsi_idr_add_path(char* path) {
    
}

void fsi_idr_remove_path(char* path) {
    
}