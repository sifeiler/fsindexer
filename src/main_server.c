#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "mongoose.h"
#include "../include/server/fsi_server.h"
#include "../include/fsi_utils.h"
#include "../include/fsi_indexer.h"
#include "../include/fsi_crawler.h"
#include "../include/fsi_worker.h"

#define DEFAULT_PORT "8080"

char* fsi_server_mg_str_null_terminate(struct mg_str str) {
    if(str.buf == NULL || str.len == 0) {
        return NULL;
    }

    char* str_heap = malloc(str.len + 1);
    if(str_heap == NULL) {
        return NULL;
    }

    strncpy(str_heap, str.buf, str.len);
    str_heap[str.len] = '\0';
    return str_heap;
}

fsi_t_key_value* fsi_server_extract_headers(struct mg_http_message *hm) {
    for (size_t i = 0; i < MG_MAX_HTTP_HEADERS && hm->headers[i].name.len > 0; i++) {
        struct mg_str *name = &hm->headers[i].name;
        struct mg_str *value = &hm->headers[i].value;
    }
    return NULL;
}

int fsi_server_http_extract_query_params(struct mg_http_message *hm, fsi_t_key_value** params_out, size_t* params_count_out) {
    struct mg_str query = hm->query;
    // an empty query is not an error case
    if(query.buf == NULL || query.len == 0) {
        *params_out = NULL;
        *params_count_out = 0;
        return 0;
    }

    if(query.buf[0] == '&' || query.buf[0] == '=') {
        printf("Invalid query.");
        return -1;
    }

    //at this point, we expect at least one query parameter
    size_t query_count = 1;
    for(size_t i = 0; i < query.len; i++) {
        if(query.buf[i] == '&') {
            query_count++;
        }
    }

    fsi_t_key_value* pairs = malloc(sizeof(fsi_t_key_value) * query_count);
    if(pairs == NULL) {
        return -1;
    }

    char* start = query.buf;
    char* end = query.buf + query.len;
    size_t pairs_counter = 0;

    while (start < end)
    {
        fsi_t_key_value* key_value = &pairs[pairs_counter];

        char* ch_pair_end = start;
        while (ch_pair_end < end && *ch_pair_end != '&') {
            ch_pair_end++;
        }

        //find '=' within this pair only
        char *ch_eq = start;
        while (ch_eq < ch_pair_end && *ch_eq != '=') {
            ch_eq++;
        }

        char* key_ptr = NULL;
        size_t key_len;
        char* val_ptr = NULL;
        size_t val_len;

        if(*ch_eq == '=') {
            //key= or key=value
            key_len = ch_eq - start;
            key_ptr = start;

            val_len = ch_pair_end - (ch_eq + 1);
            val_ptr = ch_eq + 1;
        } else {
            //end of string reached
            key_len = ch_pair_end - start;
            key_ptr = start;

            val_ptr = NULL;
            val_len = 0;
        }

        start = ch_pair_end + 1;
        pairs_counter++;
    }

    *params_count_out = query_count;
    *params_out = pairs;

    return 0;
}

FSI_HTTP_BODY_TYPE fsi_server_get_body_type(struct mg_http_message *hm) {
    struct mg_str *ct = mg_http_get_header(hm, "Content-Type");

    if (hm->body.len == 0) {
        return HTTP_BODY_NONE;
    } else if (ct == NULL) {
        return HTTP_BODY_UNKNOWN; // body present but no content-type
    } else if (memmem(ct->buf, ct->len, "application/json", strlen("application/json")) != NULL) {
        return HTTP_BODY_JSON;
    } else {
        return HTTP_BODY_UNKNOWN;
    }
}

int fsi_server_http_parse_msg(struct mg_http_message *hm, fsi_t_server_http_request* request_out) {
    FSI_HTTP_METHOD method = fsi_server_parse_http_method(hm->method);
    request_out->http_method = method;

    request_out->endpoint = fsi_server_mg_str_null_terminate(hm->uri);
    request_out->endpoint_len = hm->uri.len;

    request_out->message_body = fsi_server_mg_str_null_terminate(hm->body);
    request_out->message_body_len = hm->body.len;

    request_out->message_head = fsi_server_mg_str_null_terminate(hm->head);
    request_out->message_head_len = hm->head.len;

    if(fsi_server_http_extract_query_params(hm, &request_out->query_params, &request_out->query_params_count) != 0) {
        free(request_out->endpoint);
        free(request_out->message_body);
        free(request_out->message_head);
        return -1;
    }

    request_out->body_type = fsi_server_get_body_type(hm);

    return 0;
}

static fsi_t_server_http_route* fsi_server_http_find_route(char* endpoint) {
    struct mg_str mg_endpoint = mg_str(endpoint);
    for (size_t i = 0; i < FSI_HTTP_ROUTE_COUNT; i++) {
        fsi_t_server_http_route *route = &fsi_http_routes[i];

        if (mg_match(mg_endpoint, mg_str(route->endpoint), NULL)) {
            return route;
        }
    }
    return NULL;
}

int fsi_server_http_pre_validate(struct mg_connection* c, fsi_t_server_http_request* http_request) {
    if(http_request->http_method == HTTP_METHOD_UNKNOWN) {
        mg_http_reply(c, HTTP_METHOD_NOT_ALLOWED, "", "{%m}\n", MG_ESC("Unknown http method"));
        return FSI_SERVER_INVALID_ARGS;
    }

    fsi_t_server_http_route* route = fsi_server_http_find_route(http_request->endpoint);

    if(route == NULL) {
        mg_http_reply(c, HTTP_NOT_FOUND, "", "{%m}\n", MG_ESC("Unsupported endpoint. No route handler available. See api spec."));
        return FSI_SERVER_INVALID_ARGS;
    }

    if(route->method != http_request->http_method) {
        mg_http_reply(c, HTTP_METHOD_NOT_ALLOWED, "", "{%m}\n", MG_ESC("Endpoint does not support the provided http method. See api spec."));
        return FSI_SERVER_INVALID_ARGS;
    }

    if(route->body_type != http_request->body_type) {
        mg_http_reply(c, HTTP_BAD_REQUEST, "", "{%m}\n", MG_ESC("Endpoint does not support the provided body type. See api spec."));
        return FSI_SERVER_INVALID_ARGS;
    }

    return 0;
}

// Connection event handler function
static void fsi_server_handle_http(struct mg_connection *c, struct mg_http_message *hm) {
    fsi_t_server_http_request request = {0};
    fsi_t_server_ctx* ctx = (fsi_t_server_ctx *) c->fn_data;
    //pass along context
    request.server_context = ctx;

    //request parsing to a more readable format
    if(fsi_server_http_parse_msg(hm, &request) != 0) {
        printf("There has been an issue while parsing the http request.");
        mg_http_reply(c, HTTP_BAD_REQUEST, "", "{%m:%d}\n", MG_ESC("Cannot parse http request."));
    }

    //deep validation will be done in the handlers
    //this is primarilly for checking if method is supported by endpoint etc. 
    if(fsi_server_http_pre_validate(c, &request) == 0) {
        printf("request.endpoint            %s\n", request.endpoint ? request.endpoint : NULL);
        printf("request.endpoint_len        %zu\n", request.endpoint_len);
        printf("request.message_body_len    %zu\n", request.message_body_len);
        printf("request.http_method         %d\n", request.http_method);

        //at this point we have a known route
        fsi_t_server_http_route* route = fsi_server_http_find_route(request.endpoint);

        //request execution & response
        fsi_t_server_http_response response = {0};
        if(route->request_handler(&request, &response) == FSI_SERVER_OK) {
            mg_http_reply(c, HTTP_OK, "Content-Type: text/plain\r\n", "%s\n", response.message_body);
        } else {
            //at this point, something went wrong on server side, so return a 5xx status code
            mg_http_reply(
                c,
                HTTP_INTERNAL_SERVER_ERROR,
                "", 
                "{%m}\n", 
                MG_ESC(
                    response.message_body 
                        ? response.message_body 
                        : "Something went wrong on server side. Check your logs."));
        }
    }
}

static void ev_handler_dispatcher(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        fsi_server_handle_http(c, hm);
    }
}

int fsi_server_init(fsi_t_server_ctx* ctx_out) {
    fsi_t_server_ctx server_context = {0};
    char* mos_path = "/tmp/storage.mos";
    const char* rootdir_path = "/home/simon/Documents/mos_test_images_subset";
    uint64_t records_count = 1000;
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

    ctx_out->indexer = indexer;
    return 0;
}

int main() {
    fsi_t_server_ctx ctx = {0};
    int init_status = fsi_server_init(&ctx);

    if(init_status != 0) {
        printf("Server init failed!");
        return 1;
    }

    struct mg_mgr mgr;  // Mongoose event manager. Holds all connections
    mg_mgr_init(&mgr);  // Initialise event manager
    ctx.mgr = &mgr;

    // Pass port 0 directly to Mongoose on Loopback (127.0.0.1)
    // Mongoose will let the OS pick a free port automatically.
    struct mg_connection *c = mg_http_listen(&mgr, "http://127.0.0.1:0", ev_handler_dispatcher, &ctx);

    if (c == NULL) {
        fprintf(stderr, "Failed to create Mongoose listener\n");
        return 1;
    }

    // Retrieve the actual OS-assigned port from Mongoose's listening connection socket
    int port = ntohs(c->loc.port);

    // Print "PORT:<port>" for Rust/Tauri to capture
    printf("PORT:%d\n", port);
    fflush(stdout); // Crucial: Flush immediately!

    for (;;) {
        mg_mgr_poll(&mgr, 1000);  // Infinite event loop
    }

    mg_mgr_free(ctx.mgr);
    //TODO: free indexer
    return 0;
}