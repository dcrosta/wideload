#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#include <curl/curl.h>

#include "loader.h"
#include "list.h"

inline unsigned long micros()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000000 + now.tv_usec;
}

CURL* setup(options opts)
{
    CURL* handle = curl_easy_init();
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, on_response);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, on_header);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "wideload 0.1.0");

    if (opts.fail_after != 0)
        curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, opts.fail_after);

    return handle;
}

/**
 * Make the given request, and populate rslt. If necessary, reconnect
 * and reinitialize handle (e.g. if the request timed out).
 */
void make_request(CURL** handle, result* rslt, request* req, options opts)
{
    rslt->req = req;

    curl_easy_setopt(*handle, CURLOPT_URL, req->url);
    curl_easy_setopt(*handle, CURLOPT_WRITEDATA, rslt);
    curl_easy_setopt(*handle, CURLOPT_HEADERDATA, rslt);

    if (req->method == HTTP_POST) {
        curl_easy_setopt(*handle, CURLOPT_POST, 1);
        curl_easy_setopt(*handle, CURLOPT_POSTFIELDS, req->payload);
        curl_easy_setopt(*handle, CURLOPT_POSTFIELDSIZE, req->payload_length);
    } else {
        curl_easy_setopt(*handle, CURLOPT_HTTPGET, 0);
    }

    if (req->num_headers > 0) {
        curl_easy_setopt(*handle, CURLOPT_HTTPHEADER, req->curl_headers);
    } else {
        curl_easy_setopt(*handle, CURLOPT_HTTPHEADER, NULL);
    }

    rslt->time_first_byte = 0;
    rslt->num_bytes = 0;
    rslt->status = 0;

    rslt->time_start = micros();
    int timeout = curl_easy_perform(*handle);
    rslt->time_end = micros();

    if (rslt->time_first_byte == 0)
        rslt->time_first_byte = rslt->time_end;

    if (timeout) {
        // Force a reconnect, as the wire may now contain
        // bytes we haven't read from this failed request
        curl_easy_cleanup(*handle);
        *handle = setup(opts);

        // Non-standard status 598 is used by some proxies
        // to indicate a read timeout. Close enough.
        rslt->status = 598;
    }
}

/**
 * Load test worker thread
 */
void* load_thread(void* st)
{
    threadstate* state = (threadstate*)st;
    request* reqs = state->reqs;
    options opts = state->opts;
    unsigned long i = 0;
    unsigned long r = 0;
    if (opts.randomize)
        r = rand() % state->req_count;

    CURL* handle = setup(opts);

    if (opts.run_requests) {
        state->rslts = malloc(sizeof(result) * opts.run_requests);

        while (i < opts.run_requests) {
            make_request(&handle, &state->rslts[i], &reqs[(r + i) % state->req_count], opts);
            i++;
        }

        state->rslt_count = i;
    } else if (opts.run_seconds) {
        unsigned long end_time = micros() + (1000000 * opts.run_seconds);

        list* resultlist = list_new();
        node* n;
        result resultbuf[1000];
        result* resultcpy;

        while (micros() < end_time) {
            make_request(&handle, &resultbuf[i % 1000], &reqs[(r + i) % state->req_count], opts);

            if (i % 1000 == 999) {
                // copy the resultbuf and count into a list node,
                // and recycle the resultbuf for the next batch
                resultcpy = malloc(1000 * sizeof(result));
                memcpy(resultcpy, resultbuf, 1000 * sizeof(result));
                n = list_node_new(resultcpy);
                list_push(resultlist, n);
            }

            i++;
        }

        state->rslt_count = 0;

        n = resultlist->head;
        while (n != NULL) {
            state->rslt_count += 1000;
            n = n->next;
        }
        // account for any left in resultbuf
        state->rslt_count += i % 1000;
        state->rslts = malloc(sizeof(result) * state->rslt_count);

        unsigned long offset = 0;
        n = resultlist->head;
        while (n != NULL) {
            memcpy(&state->rslts[offset], n->data, 1000 * sizeof(result));
            offset += 1000;
            n = n->next;
        }
        // copy any left in resultbuf
        memcpy(&state->rslts[offset], resultbuf, (i % 1000) * sizeof(result));

        list_free(resultlist, 1);
    }

    curl_easy_cleanup(handle);

    return NULL;
}

/**
 * Called by libcurl as each chunk of response is received from the server.
 */
size_t on_response(void* buffer, size_t size, size_t nmemb, void* ctx)
{
    unsigned long num_bytes = size * nmemb;
    result* rslt = (result *)ctx;

    rslt->num_bytes += num_bytes;

    return num_bytes;
}

/**
 * Called by libcurl as each header is received from the server.
 */
size_t on_header(void* buffer, size_t size, size_t nmemb, void* ctx)
{
    unsigned long num_bytes = size * nmemb;
    result* rslt = (result *)ctx;

    if (rslt->status == 0) {
        // Parse the status from "HTTP/1.X NNN message"
        char * tmp = buffer;
        char * status;
        strsep(&tmp, " ");
        status = tmp;
        strsep(&tmp, " \r\n");

        rslt->status = strtol(status, NULL, 10);
        rslt->time_first_byte = micros();
    }

    return num_bytes;
}
