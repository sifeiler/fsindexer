#include <inttypes.h>
#include "../include/fsi_types_fwd.h"
#include "../include/fsi_indexer.h"
#include "../include/fsi_crawler.h"
#include "../include/fsi_worker.h"
#include "../include/fsi_utils.h"
#include "../include/fsi_queue.h"
#include "../include/fsi_clip.h"

#include <stddef.h>

//TODO; only include mos.h
#include "mos.h"
#include "mos_qry.h"
#include "mos_math.h"
#include "mos_internal.h"

void fsi_idr_process_storage_queue(fsi_t_idr_indexer* indexer);

fsi_t_idr_indexer* fsi_idr_create(fsi_t_idr_config* cfg) {
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

    uint64_t fsi_file_data_size = sizeof(fsi_t_file_data);
    //TODO: change size to size of storage record struct
    context->storage_queue = fsi_queue_create(50000, fsi_file_data_size);
    if(context->storage_queue == NULL) {
        return NULL;
    }

    //connect queues
    //crawler owns file queue, worker reads from file queue
    context->worker->context.file_queue = context->crawler->context.file_queue;
    context->worker->context.storage_queue = context->storage_queue;

    mos_t_attr_config file_id_attr = {
        .name = "file_id",
        .type = MOS_ATTR_TYPE_UINT64,
        .byte_size = 8,
        .field_offset = offsetof(fsi_t_mos_file, file_id),
        .indexed = 0
    };
    mos_t_attr_config file_name_attr = {
        .name = "file_name",
        .type = MOS_ATTR_TYPE_STRING,
        .byte_size = sizeof(mos_t_string),
        .field_offset = offsetof(fsi_t_mos_file, file_name),
        .indexed = 1
    };
    mos_t_attr_config file_path_attr = {
        .name = "file_path",
        .type = MOS_ATTR_TYPE_STRING,
        .byte_size = sizeof(mos_t_string),
        .field_offset = offsetof(fsi_t_mos_file, file_path),
        .indexed = 0
    };
    mos_t_attr_config file_vector_attr = {
        .name = "vector",
        .type = MOS_ATTR_TYPE_VECTOR,
        .byte_size = sizeof(mos_t_float_vector),
        .field_offset = offsetof(fsi_t_mos_file, vector),
        .indexed = 1
    };

    mos_t_attr_config attributes[4];
    attributes[0] = file_id_attr;
    attributes[1] = file_name_attr;
    attributes[2] = file_path_attr;
    attributes[3] = file_vector_attr;

    mos_t_idx_config file_name_idx = {
        .attribute_name = "file_name",
        .type = MOS_IDX_HASH_MAP
    };

    mos_t_idx_config file_vector_idx = {
        .attribute_name = "vector",
        .type = MOS_IDX_HNSW,
        .params.hnsw = {
            .vector_dim = FSI_VECTOR_MAX_DIM,
            .vector_metric = METRIC_COSINE,
            .graph_config = {
                .m = 16,
                .m_max0 = 32,
                .ef_construction = 128,
                .mL = 0.36f
            }
        }
    };

    mos_t_idx_config indexes[2];
    indexes[0] = file_name_idx;
    indexes[1] = file_vector_idx;

    mos_t_storage_config mos_cfg = {
        .max_records = cfg->records_count,
        .attribute_count = 4,
        .index_count = 2,
        .attributes = attributes,
        .padded_record_byte_size = sizeof(fsi_t_mos_file),
        .indexes = indexes
    };
    strcpy(mos_cfg.storage_path, cfg->storage_path);

    mos_t_storage* storage = mos_create_storage(mos_cfg.storage_path, &mos_cfg);
    if(storage == NULL) {
        printf("Failed to create indexer. Cannot allocate storage.");
        return NULL;
    }

    indexer->context.storage = storage;

    const char *model_path = FSI_MODEL_DIR "/clip-vit-base-patch32.gguf";
    fsi_clip_global_init();
    fsi_clip_context_load(model_path, &indexer->context.clip_context);

    return indexer;
}

void fsi_idr_start(fsi_t_idr_indexer* indexer, const char** root_paths, size_t path_count) {
    //queues are created, crawler and worker are waiting for each other, so order of starting should not matter
    fsi_wrk_start_worker(indexer->context.worker);
    fsi_cwl_crawler_init(indexer->context.crawler, root_paths, path_count);
    fsi_cwl_crawler_start(indexer->context.crawler);
    fsi_idr_process_storage_queue(indexer);
}

void fsi_idr_process_storage_queue(fsi_t_idr_indexer* indexer) {
    printf("Baseline indexing: start\n");
    fsi_t_file_data file_data = {0};
    uint64_t file_count = 0;
    while(fsi_queue_pop(indexer->context.storage_queue, &file_data, FSI_QUEUE_BLOCKING) == FSI_QUEUE_OK) {
        fsi_t_mos_file mos_file = {
            .file_id = file_data.file_info.file_id,
            .file_name = {
                .str = file_data.file_info.file_name,
                .str_len = file_data.file_info.file_name_len
            },
            .file_path = {
                .str = file_data.file_info.file_path,
                .str_len = file_data.file_info.file_path_len
            },
            .vector = {
                .vector = file_data.file_info.vector,
                .vector_dim = FSI_VECTOR_MAX_DIM
            }
        };

        if (mos_file.vector.vector == NULL) {
            printf("[!!!] NULL vector for file_id=%llu path=%s — skipping or investigate upstream\n",
                   (unsigned long long)mos_file.file_id,
                   file_data.file_info.file_path);
        }

        mos_storage_put(indexer->context.storage, mos_file.file_id, &mos_file);
        file_count++;

        if((file_count % 10000) == 0) {
            printf("[storage] Processed another 10000 files. %" PRIu64 " files processed\n", file_count);
        }
    }
    //wait for all threads to finish
    fsi_cwl_join_crawler(indexer->context.crawler);
    fsi_wrk_join_worker(indexer->context.worker);

    printf("Baseline indexing: done\n");
    printf("--------------------------------------------------------\n");

    /**
    struct clip_ctx* clip_context;
    const char *model_path = FSI_MODEL_DIR "/clip-vit-base-patch32.gguf";
    fsi_clip_context_load(model_path, &clip_context);

    printf("=== Interactive Vector Search ===\n");
    printf("Usage:\n");
    printf("  [threshold] text: <prompt>   (e.g., '0.72 text: a red car')\n");
    printf("  [threshold] image: <path>    (e.g., '0.30 image: /path/img.jpg')\n");
    printf("  (Defaults: Image threshold = 0.35, Text threshold = 0.75)\n\n");
    char input[FSI_PATH_MAX] = {0};

    while (fgets(input, sizeof(input), stdin) != NULL) {
        input[strcspn(input, "\r\n")] = '\0';
        if (input[0] == '\0') {
            continue;
        }

        float user_threshold = -1.0f; // Marker for unassigned threshold
        char* cursor = input;

        // 1. Parse optional float threshold at the beginning
        char* endptr = NULL;
        float parsed_val = strtof(cursor, &endptr);
        if (endptr != cursor && (*endptr == ' ' || *endptr == '\0')) {
            user_threshold = parsed_val;
            cursor = endptr;
            while (*cursor == ' ') cursor++; // Consume trailing spaces
        }

        // 2. Parse Mode Prefix and Payload
        bool is_text_search = false;
        const char* query_payload = cursor;

        if (strncmp(cursor, "text:", 5) == 0) {
            is_text_search = true;
            query_payload = cursor + 5;
            while (*query_payload == ' ') query_payload++;
        } else if (strncmp(cursor, "image:", 6) == 0) {
            is_text_search = false;
            query_payload = cursor + 6;
            while (*query_payload == ' ') query_payload++;
        } else {
            // Fallback: Check if path exists, otherwise treat as text query
            FILE* f = fopen(cursor, "rb");
            if (f) {
                fclose(f);
                is_text_search = false;
            } else {
                is_text_search = true;
            }
        }

        // 3. Assign Default Thresholds if omitted by user
        if (user_threshold < 0.0f) {
            user_threshold = is_text_search ? 0.75f : 0.35f;
        }

        // 4. Extract Vector Embedding
        float prompt_vector[FSI_VECTOR_MAX_DIM];
        if (is_text_search) {
            printf("[SEARCH] Mode: TEXT | Threshold: %.2f | Prompt: '%s'\n", user_threshold, query_payload);
            fsi_clip_text_get_vector_f32(clip_context, query_payload, prompt_vector, FSI_VECTOR_MAX_DIM);
        } else {
            printf("[SEARCH] Mode: IMAGE | Threshold: %.2f | Path: '%s'\n", user_threshold, query_payload);
            fsi_clip_image_get_vector_f32(clip_context, query_payload, prompt_vector, FSI_VECTOR_MAX_DIM);
        }

        // 5. Construct Search Query Struct with Dynamic Threshold
        mos_t_qry_search_step prompt_step = {
            .op = MOS_QRY_OP_SIMILAR,
            .attribute_query = {
                .attribute_name = "vector",
                .value = {
                    .type = MOS_ATTR_TYPE_VECTOR,
                    .byte_length = FSI_VECTOR_MAX_DIM * sizeof(float),
                    .vector_val = {
                        .ef = 64,
                        .top_k = 5,
                        .threshold = user_threshold, // <--- Dynamic value inserted here
                        .vector_dim = FSI_VECTOR_MAX_DIM,
                        .vector_val = prompt_vector
                    }
                }
            }
        };

        mos_t_qry query = {
            .query = &prompt_step
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

        printf("[SEARCH] Found items: %" PRId64 "\n", actual_ones);

        for (size_t i = 0; i < actual_ones; i++) {
            int64_t row_id = row_ids_buffer[i];
            fsi_t_mos_file* file = (fsi_t_mos_file*)mos_storage_get_data_for_row_id(indexer->context.storage, row_id);
            printf("[SEARCH] Found path: '%.*s'\n", file->file_path.str_len, file->file_path.str);
        }
        free(row_ids_buffer);
        memset(input, 0, FSI_PATH_MAX);
    }
    fsi_clip_context_free(clip_context);
    */
}

mos_t_qry_search_step* fsi_idr_fsi_to_mos_query(fsi_t_file_query* fsi_query, struct clip_ctx* clip_context) {
    mos_t_qry_search_step* mos_query = calloc(1, sizeof(mos_t_qry_search_step));
    if(mos_query == NULL) {
        printf("Failed to allocate memory for mos_t_qry_search_step. Query parsing failed.");
        return NULL;
    }

    if(fsi_query->operator == MOS_QRY_OP_OR || fsi_query->operator == MOS_QRY_OP_AND) {
        size_t sub_query_count = fsi_query->data.logical.sub_query_count;
        mos_t_qry_search_step** sub_steps = calloc(1, sub_query_count * sizeof(mos_t_qry_search_step*));
        
        if(sub_steps == NULL) {
            printf("Failed to allocate memory for sub_steps. Query parsing failed.");
            return NULL;
        }

        for(size_t i = 0; i < sub_query_count; i++) {
            fsi_t_file_query* sub_query = fsi_query->data.logical.sub_queries[i];

            if(sub_query == NULL) {
                printf("Could not parse subquery. Query parsing failed.");
                return NULL;
            }

            sub_steps[i] = fsi_idr_fsi_to_mos_query(sub_query, clip_context);
        }

        mos_query->sub_steps = sub_steps;
        mos_query->step_count = sub_query_count;
        mos_query->op = fsi_query->operator;
        return mos_query;
    }

    if(fsi_query->operator == MOS_QRY_OP_NOT) {
        mos_query->op = MOS_QRY_OP_NOT;
        mos_query->step_count = 1;

        mos_t_qry_search_step *sub_step = fsi_idr_fsi_to_mos_query(fsi_query->data.not.sub_query, clip_context);

        mos_query->sub_steps = malloc(sizeof(mos_t_qry_search_step*));

        if(mos_query->sub_steps == NULL) {
            printf("Failed to allocate memory for sub_step. Query parsing failed.");
            return NULL;
        }

        if (!mos_query->sub_steps) { /* handle allocation failure */ }
        mos_query->sub_steps[0] = sub_step;

        return mos_query;
    }

    fsi_t_attribute_query* attribute_query = &fsi_query->data.relational;

    // at this point it's a relational query
    memcpy(mos_query->attribute_query.attribute_name, fsi_query->data.relational.attribute_name, fsi_query->data.relational.attribute_name_len);
    mos_query->op = fsi_query->operator;
    mos_query->step_count = 0;

    mos_t_attr_value* mos_attr_value = &mos_query->attribute_query.value;
    
    if(attribute_query->operator == MOS_QRY_OP_SIMILAR) {
        fsi_t_qry_value_similar vector_query = attribute_query->value.vector_val;
        size_t vector_size_bytes = sizeof(float) * vector_query.vector_dim;

        float *vector_buf = malloc(vector_size_bytes);
        if (vector_buf == NULL) {
            return NULL; 
        }

        if(attribute_query->value_type == VALUE_TYPE_IMAGE_BASE64) {
            //image is part of char_val as a base64 representation
            // get configured vector dimension from mos, as the user can provide whatever number
            fsi_clip_image_base64_get_vector_f32(clip_context, vector_query.char_val, strlen(vector_query.char_val), vector_buf, vector_query.vector_dim);
        } else if(attribute_query->value_type == VALUE_TYPE_IMAGE_PROMPT) {
            fsi_clip_text_get_vector_f32(clip_context, vector_query.char_val, vector_buf, vector_query.vector_dim);
        } else if(attribute_query->value_type == VALUE_TYPE_IMAGE_PATH) {
            fsi_clip_image_get_vector_f32(clip_context, vector_query.char_val, vector_buf, vector_query.vector_dim);
        }

        mos_attr_value->byte_length = sizeof(float) * vector_query.vector_dim;
        mos_attr_value->type = MOS_ATTR_TYPE_VECTOR;

        mos_attr_value->vector_val.vector_val = vector_buf;
        mos_attr_value->vector_val.vector_dim = vector_query.vector_dim;
        mos_attr_value->vector_val.ef = vector_query.ef;
        mos_attr_value->vector_val.threshold = vector_query.threshold;
        mos_attr_value->vector_val.top_k = vector_query.top_k;
    }
    
    return mos_query;
}

uint64_t fsi_idr_query(fsi_t_idr_indexer* indexer, fsi_t_file_query* query, fsi_t_mos_file*** result_out) {
    *result_out = NULL;

    // map query to mos query
    mos_t_qry_search_step* outer_qry_step = fsi_idr_fsi_to_mos_query(query, indexer->context.clip_context);

    if(outer_qry_step == NULL) {
        return 0;
    }

    mos_t_qry mos_qry = {
        .query = outer_qry_step
    };

    const mos_t_qry_bmp* bmp = mos_storage_search(indexer->context.storage, &mos_qry);
    uint64_t expected_ones = mos_qry_bmp_count_ones(bmp);

    if (expected_ones == 0) {
        return 0; // nothing found, *result_out stays NULL
    }

    int64_t* row_ids_buffer = malloc(expected_ones * sizeof(int64_t));
    if (!row_ids_buffer) {
        printf("Cannot allocate row_ids_buffer.\n");
        return 0;
    }

    uint64_t actual_ones = mos_qry_bmp_get_row_ids(bmp, row_ids_buffer);
    if (expected_ones != actual_ones) {
        printf("Result bitmap mismatch. Expected %" PRIu64 ", was %" PRIu64 "\n", expected_ones, actual_ones);
        free(row_ids_buffer);
        return 0;
    }

    fsi_t_mos_file** files = malloc(actual_ones * sizeof(fsi_t_mos_file*));
    if (!files) {
        printf("Cannot allocate files array.\n");
        free(row_ids_buffer);
        return 0;
    }

    for (size_t i = 0; i < actual_ones; i++) {
        int64_t row_id = row_ids_buffer[i];
        files[i] = (fsi_t_mos_file*) mos_storage_get_data_for_row_id(indexer->context.storage, row_id);
    }

    free(row_ids_buffer);
    *result_out = files;
    return actual_ones;
}

void fsi_idr_free_query_result(fsi_t_mos_file** files, uint64_t count) {
    if (!files) return;

    for (uint64_t i = 0; i < count; i++) {
        if (files[i]) {
            free(files[i]);
        }
    }
    free(files);
}

void fsi_idr_stop(fsi_t_idr_indexer* indexer) {
    
}

void fsi_idr_add_path(char* path) {
    
}

void fsi_idr_remove_path(char* path) {
    
}