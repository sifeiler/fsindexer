#ifndef FSI_SERVER_H
#define FSI_SERVER_H

#include "mongoose.h"
#include "../fsi_indexer.h"

// general API
#define API_ROOT "fsi"

// Config resource
#define API_CONFIG "config"
#define ENDPOINT_CONFIG_GET "/" API_ROOT "/" API_CONFIG
#define ENDPOINT_CONFIG_POST "/" API_ROOT "/" API_CONFIG

// File resource
#define API_FILE "file"
#define ENDPOINT_FILE_QUERY_POST "/" API_ROOT "/" API_FILE "/query"

typedef enum FSI_HTTP_METHOD {
    HTTP_METHOD_UNKNOWN = 0,
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_PATCH,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_OPTIONS
} FSI_HTTP_METHOD;

typedef enum FSI_HTTP_STATUS {
    // 2xx Success
    HTTP_OK                    = 200,
    HTTP_CREATED               = 201,
    HTTP_ACCEPTED              = 202,
    HTTP_NO_CONTENT            = 204,

    // 4xx Client Errors
    HTTP_BAD_REQUEST           = 400,
    HTTP_UNAUTHORIZED          = 401,
    HTTP_FORBIDDEN             = 403,
    HTTP_NOT_FOUND             = 404,
    HTTP_METHOD_NOT_ALLOWED    = 405,
    HTTP_CONFLICT              = 409,
    HTTP_UNPROCESSABLE_ENTITY  = 422,

    // 5xx Server Errors
    HTTP_INTERNAL_SERVER_ERROR = 500,
    HTTP_NOT_IMPLEMENTED       = 501,
    HTTP_SERVICE_UNAVAILABLE   = 503
} FSI_HTTP_STATUS;

typedef enum FSI_HTTP_BODY_TYPE {
    HTTP_BODY_NONE = 0,
    HTTP_BODY_UNKNOW = 1,
    HTTP_BODY_MULTIPART = 2,
    HTTP_BODY_TEXT = 3,
    HTTP_BODY_JSON = 4,
    HTTP_BODY_UNKNOWN = 5,
    HTTP_BODY_URLENCODED = 6,
    HTTP_BODY_BINARY = 7
} FSI_HTTP_BODY_TYPE;

static inline FSI_HTTP_METHOD fsi_server_parse_http_method(struct mg_str method) {
    if (mg_match(method, mg_str("GET"), NULL))  return HTTP_METHOD_GET;
    if (mg_match(method, mg_str("POST"), NULL))  return HTTP_METHOD_POST;
    if (mg_match(method, mg_str("PUT"), NULL))  return HTTP_METHOD_PUT;
    if (mg_match(method, mg_str("DELETE"), NULL))  return HTTP_METHOD_DELETE;
    if (mg_match(method, mg_str("PATCH"), NULL))  return HTTP_METHOD_PATCH;
    if (mg_match(method, mg_str("HEAD"), NULL))  return HTTP_METHOD_HEAD;
    if (mg_match(method, mg_str("OPTIONS"), NULL))  return HTTP_METHOD_OPTIONS;
    return HTTP_METHOD_UNKNOWN;
}

typedef struct fsi_t_server_ctx {
    fsi_t_idr_indexer* indexer;
    struct mg_mgr* mgr;
} fsi_t_server_ctx;

typedef struct fsi_t_key_value {
    char* key;      //null terminated
    size_t key_len;
    char* value;    //null terminated
    size_t value_len;
} fsi_t_key_value;

typedef struct fsi_t_server_http_request {
    char* endpoint;
    size_t endpoint_len;
    FSI_HTTP_METHOD http_method;
    fsi_t_key_value* query_params;
    size_t query_params_count;
    char* message_head;     //null terminated
    size_t message_head_len;
    char* message_body;     //null terminated
    size_t message_body_len;
    FSI_HTTP_BODY_TYPE body_type;
    fsi_t_server_ctx* server_context;
} fsi_t_server_http_request;

typedef struct fsi_t_server_http_response {
    FSI_HTTP_STATUS status;
    char* message_body;     //null terminated
    size_t message_body_len;
} fsi_t_server_http_response;

typedef enum FSI_SERVER_STATUS {
    FSI_SERVER_OK = 0,
    FSI_SERVER_INVALID_ARGS = 1
} FSI_SERVER_STATUS;

typedef struct fsi_t_server_http_route {
    char* endpoint;
    FSI_HTTP_METHOD method;
    FSI_HTTP_BODY_TYPE body_type;
    FSI_SERVER_STATUS (*request_handler)(fsi_t_server_http_request* request, fsi_t_server_http_response* response_out);
} fsi_t_server_http_route;

FSI_SERVER_STATUS fsi_server_http_handle_file_query_request(fsi_t_server_http_request* request, fsi_t_server_http_response* response_out);

/**
 * Dispatches the request to a request handler and provides the request handlers result as a response out param.
 * 
 * @param request the request. Will be dispatched to the correct request handler.
 * @param respone_out the response to the request. Contains all information needed to answer the client.
 */
FSI_SERVER_STATUS fsi_server_http_request(fsi_t_server_http_request* request, fsi_t_server_http_response* response_out);

static fsi_t_server_http_route fsi_http_routes[] = {
    { ENDPOINT_FILE_QUERY_POST, HTTP_METHOD_POST, HTTP_BODY_JSON, fsi_server_http_handle_file_query_request }
};

// is this hacky? Anyways... don't want to keep a manual counter...
#define FSI_HTTP_ROUTE_COUNT (sizeof(fsi_http_routes) / sizeof(fsi_http_routes[0]))

#endif