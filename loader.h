#ifndef WIDELOAD_LOADER_H
#define WIDELOAD_LOADER_H

#include "cli.h"

typedef enum {
    HTTP_GET,
    HTTP_POST
} http_method;

typedef struct {
    http_method   method;
    char*         url;
    unsigned long payload_length;
    char*         payload;
} request;

typedef struct {
    unsigned long count;
    request*      reqs;
} requests;

typedef struct {
    request*      req;
    int           status;
    unsigned long num_bytes;

    unsigned long time_start;
    unsigned long time_first_byte;
    unsigned long time_end;
} result;

typedef struct {
    options       opts;

    unsigned long req_count;
    request*      reqs;

    unsigned long rslt_count;
    result*       rslts;
} threadstate;


/**
 * Main thread entry point.
 */
void* load_thread(void* arg);

/**
 * Called by libcurl as each chunk of response is received from the server.
 */
size_t on_response(void* buffer, size_t size, size_t nmemb, void* ctx);

/**
 * Called by libcurl as each header is received from the server.
 */
size_t on_header(void* buffer, size_t size, size_t nmemb, void* ctx);

#endif
