#ifndef FSI_INDEXER_H
#define FSI_INDEXER_H

#include <inttypes.h>
#include "fsi_types_fwd.h"
#include <stddef.h>
#include "clip.h"

#include "fsi_os.h"

#include "mos.h"
#include "mos_string.h"
#include "mos_qry.h"

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
    struct clip_ctx* clip_context;
} fsi_t_context;

typedef struct fsi_t_idr_indexer {
    fsi_t_context context;
} fsi_t_idr_indexer;

// Record bytes:
// file_id      8 bytes
// file_name   12 bytes for the string descriptor (actual string is stored in silo)
// file_path   12 bytes for the string descriptor (actual string is stored in silo)
// vector       8 bytes for the node_id of the vector index. Vector is not directly stored with records.
//             --------
//             40 bytes in total + padding
typedef struct fsi_t_mos_file {
    uint64_t file_id;
    mos_t_string file_name;
    mos_t_string file_path;
    mos_t_float_vector vector;
} fsi_t_mos_file;

typedef struct fsi_t_file_data {
    fsi_file_info file_info;
} fsi_t_file_data;

typedef enum FSI_QRY_OPERATOR {
    FSI_QRY_OP_OR   = 1 << 0,
    FSI_QRY_OP_AND  = 1 << 1,
    FSI_QRY_OP_NOT  = 1 << 2,
    FSI_QRY_OP_EQ   = 1 << 3,
    FSI_QRY_OP_SIMILAR   = 1 << 4
} FSI_QRY_OPERATOR;

#define FSI_LOGICAL_OP (FSI_QRY_OP_OR | FSI_QRY_OP_AND | FSI_QRY_OP_NOT)
#define FSI_RELATIONAL_OP (FSI_QRY_OP_EQ | FSI_QRY_OP_SIMILAR)

typedef enum FSI_QRY_VALUE_TYPE {
    VALUE_TYPE_IMAGE_PATH,
    VALUE_TYPE_IMAGE_PROMPT,
    VALUE_TYPE_IMAGE_BASE64,
    VALUE_TYPE_VECTOR,
    VALUE_TYPE_TEXT,
    VALUE_TYPE_INT64,
    VALUE_TYPE_UNKNOWN
} FSI_QRY_VALUE_TYPE;

typedef struct fsi_t_qry_value_similar {
    char* char_val;
    float* vector_val;
    uint64_t vector_dim;    // depends on embedding (used model)
    uint64_t top_k;         // Return at most k results,
    float threshold;        // but only results with similarity >= threshold
    uint16_t ef;            // Must be >= k, the number of neighbors requested. Higher ef = better recall, slower search.
} fsi_t_qry_value_similar;

typedef struct fsi_t_attribute_query {
    char* attribute_name;
    size_t attribute_name_len;
    FSI_QRY_OPERATOR operator;
    FSI_QRY_VALUE_TYPE value_type;
    union {
        char* char_val;
        uint64_t int64_val;
        fsi_t_qry_value_similar vector_val;
    } value;
    size_t value_len;
} fsi_t_attribute_query;

typedef struct fsi_t_file_query fsi_t_file_query;

typedef struct fsi_t_file_query {
    FSI_QRY_OPERATOR operator;
    union {
        struct {
            fsi_t_file_query** sub_queries;
            size_t sub_query_count;
        } logical;

        struct {
            fsi_t_file_query* sub_query;
        } not;

        fsi_t_attribute_query relational;
    } data;
} fsi_t_file_query;

static MOS_QRY_OPERATOR fsi_to_mos_operator_mapping[] = {
    [FSI_QRY_OP_OR] = MOS_QRY_OP_OR,
    [FSI_QRY_OP_AND] = MOS_QRY_OP_AND,
    [FSI_QRY_OP_NOT] = MOS_QRY_OP_NOT,
    [FSI_QRY_OP_EQ] = MOS_QRY_OP_EQ,
    [FSI_QRY_OP_SIMILAR] = MOS_QRY_OP_SIMILAR
};

fsi_t_idr_indexer* fsi_idr_create(fsi_t_idr_config* cfg);
void fsi_idr_start(fsi_t_idr_indexer* indexer, const char** root_paths, size_t path_count);
void fsi_idr_stop(fsi_t_idr_indexer* indexer);
void fsi_idr_add_path(char* path);
void fsi_idr_remove_path(char* path);

uint64_t fsi_idr_query(fsi_t_idr_indexer* indexer, fsi_t_file_query* query, fsi_t_mos_file*** result_out);
void fsi_idr_free_query_result(fsi_t_mos_file** files, uint64_t count);

#endif