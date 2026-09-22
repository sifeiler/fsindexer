#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/server/fsi_server.h"
#include "../../include/fsi_indexer.h"
#include "cJSON.h"

MOS_QRY_OPERATOR fsi_server_get_query_operator(char* operator) {
    if(strcmp(operator, "OR") == 0) {
        return MOS_QRY_OP_OR;
    } else if(strcmp(operator, "AND") == 0) {
        return MOS_QRY_OP_AND;
    } else if(strcmp(operator, "NOT") == 0) {
        return MOS_QRY_OP_NOT;
    } else if(strcmp(operator, "EQ") == 0) {
        return MOS_QRY_OP_EQ;
    } else if(strcmp(operator, "SIMILAR") == 0) {
        return MOS_QRY_OP_SIMILAR;
    }
    return -1;
}

FSI_QRY_VALUE_TYPE fsi_server_get_value_type(cJSON* cjson) {
    if(strcmp(cjson->valuestring, "image_path") == 0) {
        return VALUE_TYPE_IMAGE_PATH;
    } else if(strcmp(cjson->valuestring, "image_prompt") == 0) {
        return VALUE_TYPE_IMAGE_PROMPT;
    } else if(strcmp(cjson->valuestring, "image_base64") == 0) {
        return VALUE_TYPE_IMAGE_BASE64;
    } else if(strcmp(cjson->valuestring, "vector") == 0) {
        return VALUE_TYPE_VECTOR;
    } else if(strcmp(cjson->valuestring, "text") == 0) {
        return VALUE_TYPE_TEXT;
    } else if(strcmp(cjson->valuestring, "int64") == 0) {
        return VALUE_TYPE_INT64;
    } else {
        return VALUE_TYPE_UNKNOWN;
    }
}

/**
 * Example:
 * {
 *  "OR": [
 *      {
 *          "attribute_name": "file_name",
 *          "operator": "EQ",
 *          "value_type": "text",
 *          "value": "file1"
 *      },
 *      {
 *          "attribute_name": "vector",
 *          "operator": "SIMILAR",
 *          "value_type": "image_path",
 *          "value": {
 *              "image_path": "/path/to/image",
 *              "ef": 64,
 *              "top_k": 5,
 *              "threshold": 0.7
 *          }
 *      },
 *      "AND": [...]
 *  ]
 * }
 */
fsi_t_file_query* fsi_server_parse_file_query_json(cJSON* cjson) {
    if(cjson == NULL) {
        return NULL;
    }

    fsi_t_file_query* query = calloc(1, sizeof(fsi_t_file_query));
    if(query == NULL) {
        printf("Could not allocate memory for query. Query parsing failed.\n");
        return NULL;
    }

    cJSON* operator = cJSON_GetObjectItemCaseSensitive(cjson, "operator");

    if(!operator) {
        printf("Operator is missing. Query parsing failed.");
        return NULL;
    }

    MOS_QRY_OPERATOR q_op = fsi_server_get_query_operator(operator->valuestring);

    if(q_op == MOS_QRY_OP_OR || q_op == MOS_QRY_OP_AND) {
        cJSON* sub_queries = cJSON_GetObjectItemCaseSensitive(cjson, "sub_queries");
        
        if(!sub_queries) {
            printf("Query operator is logical, but sub_queries is missing. Query parsing failed.\n");
            return NULL;
        }

        if(!cJSON_IsArray(sub_queries)) {
            printf("Logical AND/OR query should be of type array. Query parsing failed.\n");
            return NULL;
        }

        query->operator = q_op;
        size_t array_size = cJSON_GetArraySize(sub_queries);

        if(array_size > 0) {
            //only allocate pointers. Real struct will be allocated in recursive call.
            query->data.logical.sub_queries = calloc(1, array_size * sizeof(fsi_t_file_query*));
            query->data.logical.sub_query_count = array_size;

            for(int i = 0; i < array_size; i++) {
                *(query->data.logical.sub_queries + i) = fsi_server_parse_file_query_json(cJSON_GetArrayItem(sub_queries, i));
            }
        }
        return query;
    }

    if(q_op == MOS_QRY_OP_NOT) {
        query->operator = MOS_QRY_OP_NOT;
        cJSON* sub_query = cJSON_GetObjectItemCaseSensitive(cjson, "sub_query");

        if(!cJSON_IsObject(sub_query)) {
            printf("Logical NOT query should be of type object. Query parsing failed.\n");
            return NULL;
        }

        query->data.not.sub_query = fsi_server_parse_file_query_json(sub_query);
        return query;
    }

    //at this point, we are parsing a relational query (EQ, SIMILAR, ...)
    cJSON* attribute_name = cJSON_GetObjectItemCaseSensitive(cjson, "attribute_name");
    cJSON* value = cJSON_GetObjectItemCaseSensitive(cjson, "value");

    if(!attribute_name) {
        printf("Relational attribute-query misses field attribute_name. Query parsing failed.\n");
        return NULL;
    }

    if(!value) {
        printf("Relational attribute-query misses field value. Query parsing failed.\n");
        return NULL;
    }

    if((q_op & MOS_QRY_RELATIONAL_OP) == 0) {
        printf("Query operator is not relational. Attribute queries require relational operators. Query parsing failed.\n");
        return NULL;
    }

    fsi_t_attribute_query* attribute_query = &query->data.relational;

    query->operator = q_op;
    attribute_query->operator = q_op;
    attribute_query->attribute_name = strdup(attribute_name->valuestring);
    attribute_query->attribute_name_len = strlen(attribute_query->attribute_name);

    cJSON* value_type = cJSON_GetObjectItemCaseSensitive(value, "value_type");

    if(!value_type) {
        printf("Relational attribute-query value property misses field value_type. Query parsing failed.\n");
        return NULL;
    }
    printf("value_type %s\n", value_type->valuestring);

    //this is the real value
    cJSON* value_dot_value = cJSON_GetObjectItemCaseSensitive(value, "value");

    FSI_QRY_VALUE_TYPE fsi_value_type = fsi_server_get_value_type(value_type);
    if(fsi_value_type == VALUE_TYPE_UNKNOWN) {
        printf("Value type unknown. Query parsing failed.\n");
        return NULL;
    }
    attribute_query->value_type = fsi_value_type;

    if(fsi_value_type == VALUE_TYPE_TEXT) {
        attribute_query->value.char_val = strdup(value_dot_value->valuestring);
    } else if(q_op == MOS_QRY_OP_SIMILAR) {
        fsi_t_qry_value_similar* vector_val = &attribute_query->value.vector_val;

        cJSON* ef = cJSON_GetObjectItemCaseSensitive(value, "ef");
        cJSON* top_k = cJSON_GetObjectItemCaseSensitive(value, "top_k");
        cJSON* threshold = cJSON_GetObjectItemCaseSensitive(value, "threshold");

        if(!ef || !top_k || !threshold) {
            printf("One or more fields of 'ef', 'top_k' or 'threshold' is missing. Query parsing failed.\n");
        }

        vector_val->ef = ef->valueint;
        vector_val->top_k = top_k->valueint;
        vector_val->threshold = threshold->valuedouble;
        vector_val->vector_dim = FSI_VECTOR_MAX_DIM;

        cJSON* internal_value = NULL;
        if(fsi_value_type == VALUE_TYPE_IMAGE_BASE64 
            || fsi_value_type == VALUE_TYPE_IMAGE_PATH
            || fsi_value_type == VALUE_TYPE_IMAGE_PROMPT) {
            if(!value_dot_value) {
                printf("Value type is %d, but field 'value' is missing. Query parsing failed.\n", fsi_value_type);
                return NULL;
            }

            vector_val->char_val = strdup(value_dot_value->valuestring);
        } else if(fsi_value_type == VALUE_TYPE_VECTOR) {
            if(!value_dot_value) {
                printf("Value type is VALUE_TYPE_VECTOR, but field 'value' is missing. Query parsing failed.\n");
                return NULL;
            }
            size_t len = cJSON_GetArraySize(value_dot_value);
            vector_val->vector_dim = len;

            vector_val->vector_val = malloc(sizeof(float) * len);
            for (size_t i = 0; i < len; i++) {
                vector_val->vector_val[i] = cJSON_GetArrayItem(value_dot_value, i)->valuedouble;
            }
        }
    }
    return query;
}

int fsi_server_to_cjson(char* json_string, cJSON** cjson_out, char** err_ptr_out) {
    *cjson_out = cJSON_Parse(json_string);
    if(*cjson_out == NULL) {
        *err_ptr_out = cJSON_GetErrorPtr();
        return -1;
    }
    return FSI_SERVER_OK;
}

void fsi_server_build_http_response_error(char* error_message, FSI_HTTP_STATUS status, fsi_t_server_http_response* response_out) {
    response_out->status = status;
    char* response_message = "Error occured during http call. Status Code: %d, Message: %s";
    size_t message_len = strlen(error_message);
    message_len += strlen(response_message) + 1;

    char* final_message = calloc(1, message_len);
    if(final_message == NULL) {
        printf("Could not allocate memory for response message.");
        response_out->message_body = NULL;
        response_out->message_body_len = 0;
        return;
    }

    snprintf(final_message, message_len, response_message, status, error_message);
    response_out->message_body = final_message;
    response_out->message_body_len = message_len;
}

cJSON* fsi_server_http_file_result_to_json(fsi_t_mos_file* file) {
    cJSON* json = cJSON_CreateObject();

    //cannot directly use file_path for the json, as mos_t_strings are pointers into string silo and strings in the silo are not null-terminated
    char path_buffer[file->file_path.str_len + 1];
    snprintf(path_buffer, sizeof(path_buffer), "%s", file->file_path.str);
    char name_buffer[file->file_name.str_len + 1];
    snprintf(name_buffer, sizeof(name_buffer), "%s", file->file_name.str);

    cJSON_AddStringToObject(json, "path", path_buffer);
    cJSON_AddStringToObject(json, "name", name_buffer);
    return json;
}

char* fsi_server_http_query_result_to_json(fsi_t_mos_file** files, uint64_t files_count) {
    cJSON* root = cJSON_CreateObject();
    cJSON* files_array = cJSON_CreateArray();

    for(uint64_t i = 0; i < files_count; i++) {
        cJSON_AddItemToArray(files_array, fsi_server_http_file_result_to_json(files[i]));
    }

    cJSON_AddItemToObject(root, "results", files_array);
    cJSON_AddNumberToObject(root, "count", (double)files_count);

    char *json_str = cJSON_PrintUnformatted(root);  // caller must free
    cJSON_Delete(root); // frees the whole tree
    return json_str;
}

FSI_SERVER_STATUS fsi_server_http_handle_file_query_request(fsi_t_server_http_request* request, fsi_t_server_http_response* response_out) {
    if(request->http_method != HTTP_METHOD_POST) {
        printf("Wrong http method for call %s. Required: POST", ENDPOINT_FILE_QUERY_POST);
        fsi_server_build_http_response_error("Wrong http method. Required: POST", HTTP_METHOD_NOT_ALLOWED, response_out);
        return FSI_SERVER_INVALID_ARGS;
    }

    //parse json payload to query struct
    cJSON* cjson = NULL;
    char* err_ptr;

    if(fsi_server_to_cjson(request->message_body, &cjson, &err_ptr) != 0) {
        //show max 40 chars of the malformed json in the error message.
        char snippet[41] = {0};
        if (err_ptr != NULL) {
            size_t len = strnlen(err_ptr, 40);
            memcpy(snippet, err_ptr, len);
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "Malformed JSON near: \"%s\"", snippet);

        fsi_server_build_http_response_error(msg, HTTP_BAD_REQUEST, response_out);
        return FSI_SERVER_INVALID_ARGS;
    }

    fsi_t_file_query* query = fsi_server_parse_file_query_json(cjson);

    if(query == NULL) {
        fsi_server_build_http_response_error("Something is wrong with the json body. Cannot parse it into a query.", HTTP_BAD_REQUEST, response_out);
        return FSI_SERVER_INVALID_ARGS;
    }
    
    //run query
    fsi_t_mos_file** files;
    uint64_t result_count = fsi_idr_query(request->server_context->indexer, query, &files);
    
    //parse query result to json payload
    response_out->message_body = fsi_server_http_query_result_to_json(files, result_count);
    response_out->message_body_len = strlen(response_out->message_body);

    fsi_idr_free_query_result(files, result_count);

    return 0;
}

FSI_SERVER_STATUS fsi_server_http_request(fsi_t_server_http_request* request, fsi_t_server_http_response* response_out) {
    if(strcmp(request->endpoint, ENDPOINT_FILE_QUERY_POST) == 0) {
        fsi_server_http_handle_file_query_request(request, response_out);
    }
    return FSI_SERVER_OK;
}