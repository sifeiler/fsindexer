#include "../include/fsi_indexer.h"
#include "../include/fsi_crawler.h"
#include "../include/fsi_worker.h"
#include <stdio.h>

int main (int argc, char** argv) {
    if(argc < 4) {
        printf("Usage: indexer.exe <storage_path> <rootdir_path> <records_count>\n");
        return 1;
    }

    char* mos_path = argv[1];
    const char* rootdir_path = argv[2];
    uint64_t records_count = strtoull(argv[3], NULL, 10);
    printf("Configuring indexer for parameters: \n");
    printf("storage file: %s \n", mos_path);
    printf("root dir: %s \n", rootdir_path);
    printf("records_count: %" PRId64 "\n", records_count);

    fsi_t_cwl_config cwl_cfg = {
        .parallelismn = 4
    };

    fsi_t_wrk_config wrk_cfg = {
        .parallelismn = 1
    };

    fsi_t_idr_config idr_cfg = {
        .crawler_cfg = &cwl_cfg,
        .worker_cfg = &wrk_cfg,
        .records_count = records_count,
        .storage_path = mos_path
    };

    fsi_t_idr_indexer* indexer = fsi_idr_create(&idr_cfg);
    if(indexer == NULL) {
        fsi_utils_report_error("Indexer null. Cannot start indexer.");
        return -1;
    }
    fsi_idr_start(indexer, &rootdir_path, 1);

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
            fsi_clip_text_get_vector_f32(indexer->context.clip_context, query_payload, prompt_vector, FSI_VECTOR_MAX_DIM);
        } else {
            printf("[SEARCH] Mode: IMAGE | Threshold: %.2f | Path: '%s'\n", user_threshold, query_payload);
            fsi_clip_image_get_vector_f32(indexer->context.clip_context, query_payload, prompt_vector, FSI_VECTOR_MAX_DIM);
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
    fsi_clip_context_free(indexer->context.clip_context);
}