#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <yaml.h>

#include "urlfile.h"
#include "list.h"
#include "base64decode.h"

/**
 * Parse a sequence of headers, and set them on the `req`.
 *
 * It is assumed that the `parser` has just encountered a
 * YAML_SCALAR_EVENT whose value was "headers", and has been
 * advanced beyond the YAML_SEQUENCE_START_EVENT; after this
 * are expected a series of YAML_SCALAR_EVENT pairs, the
 * header names and values.
 *
 * After return, the `parser` will have consumed the matching
 * YAML_SEQUENCE_END_EVENT.
 *
 * Return 1 on error or 0 on success.
 */
int parse_headers(yaml_parser_t* parser, request* req)
{
    unsigned long i;
    char* full_header;
    yaml_event_t name_event, value_event;
    list* headers;
    node* n;
    header* hdr;

    headers = list_new();

    while (1) {
        if (!yaml_parser_parse(parser, &name_event))
            goto parse_headers_error;
        while (name_event.type == YAML_MAPPING_START_EVENT || name_event.type == YAML_MAPPING_END_EVENT) {
            yaml_event_delete(&name_event);
            if (!yaml_parser_parse(parser, &name_event))
                goto parse_headers_error;
        }
        if (name_event.type == YAML_SEQUENCE_END_EVENT)
            break;
        if (name_event.type != YAML_SCALAR_EVENT)
            goto parse_headers_error;

        if (!yaml_parser_parse(parser, &value_event))
            goto parse_headers_error;
        if (value_event.type != YAML_SCALAR_EVENT) {
            fprintf(stderr, "invalid header value for '%s'\n", name_event.data.scalar.value);
            goto parse_headers_error;
        }

        if (!(hdr = malloc(sizeof(header))))
            goto parse_headers_error;
        if (!(hdr->name = malloc(sizeof(char) * (strlen((const char*)name_event.data.scalar.value) + 1))))
            goto parse_headers_error;
        if (!(hdr->value = malloc(sizeof(char) * (strlen((const char*)value_event.data.scalar.value) + 1))))
            goto parse_headers_error;

        strcpy(hdr->name, (const char*)name_event.data.scalar.value);
        strcpy(hdr->value, (const char*)value_event.data.scalar.value);
        n = list_node_new(hdr);
        list_push(headers, n);

        yaml_event_delete(&name_event);
        yaml_event_delete(&value_event);
    }

    if (!(req->headers = malloc(sizeof(header) * headers->length)))
        goto parse_headers_error;

    req->curl_headers = NULL;
    n = headers->head;
    for (i=0; i<headers->length; i++) {
        hdr = (header *)n->data;
        req->headers[i].name = hdr->name;
        req->headers[i].value = hdr->value;

        // also set up header list for libcurl
        full_header = malloc(sizeof(char) * (strlen(hdr->name) + strlen(hdr->value) + 3));
        if (!full_header)
            goto parse_headers_error;
        sprintf(full_header, "%s: %s", hdr->name, hdr->value);
        req->curl_headers = curl_slist_append(req->curl_headers, full_header);
        if (req->curl_headers == NULL)
            goto parse_headers_error;
        free(full_header);

        n = n->next;
    }
    req->num_headers = headers->length;

    list_free(headers, 0);
    return 0;

parse_headers_error:
    // printf("in parse_headers_error\n");
    list_free(headers, 1);
    return 1;
}

/**
 * Parse a (possibly base64 encoded) payload and set it on
 * the `req`.
 *
 * It is assumed that the `parser` has just encountered a
 * YAML_SCALAR_EVENT whose value was either "payload" or
 * "payload64", and the next event will be a YAML_SCALAR_EVENT
 * representing the payload itself.
 *
 * Return 1 on error or 0 on success.
 */
int parse_payload(yaml_parser_t* parser, yaml_event_t* event, request* req)
{
    char* payload;
    size_t rawlength;
    yaml_event_t payload_event;
    if (!yaml_parser_parse(parser, &payload_event))
        return 1;

    payload = (char *)payload_event.data.scalar.value;
    // printf("raw payload: '%s'\n", payload_event.data.scalar.value);
    rawlength = strlen(payload);

    if (0 == strncmp("payload64", (const char*)event->data.scalar.value, 9)) {
        unsigned long payload_length;

        if (rawlength % 4 != 0) {
            fprintf(stderr, "base64-encoded POST payload length must be a positive multiple of 4 (was %lu)", rawlength);
            yaml_event_delete(&payload_event);
            return 1;
        }

        payload_length = rawlength / 4 * 3;
        if (payload[rawlength - 1] == '=')
            payload_length--;
        if (payload[rawlength - 2] == '=')
            payload_length--;

        if (!(req->payload = malloc(sizeof(char) * (payload_length + 1)))) {
            yaml_event_delete(&payload_event);
            return 1;
        }
        req->payload[payload_length] = '\0';

        base64_decodestate state;
        base64_init_decodestate(&state);
        base64_decode_block((const char*)payload_event.data.scalar.value, rawlength, req->payload, &state);
        req->payload_length = payload_length;
    } else {
        if (!(req->payload = malloc(sizeof(char) * (rawlength + 1)))) {
            yaml_event_delete(&payload_event);
            return 1;
        }
        strcpy(req->payload, (const char*)payload_event.data.scalar.value);
        req->payload_length = rawlength;
    }

    return 0;
}

/**
 * Parse and return a single request from the YAML URLs file.
 *
 * It is assumed that the `parser` has just encountered a
 * YAML_MAPPING_START_EVENT, and the next events will be two
 * YAML_SCALAR_EVENTs, corresponding to the method and URL.
 *
 * When this function returns, the `parser` will have just
 * processed a YAML_MAPPING_END_EVENT.
 *
 * The return value is a pointer to a request, or NULL if there
 * was an error; in the event of an error, no assumption should
 * be made about the state of the parser.
 */
request* parse_request(yaml_parser_t* parser)
{
    request* req;
    unsigned long expected_ends = 1;
    unsigned long i;

    if (!(req = malloc(sizeof(request))))
        goto parse_request_error;

    req->url = NULL;
    req->payload_length = 0;
    req->payload = NULL;
    req->headers = NULL;
    req->num_headers = 0;
    req->curl_headers = NULL;

    yaml_event_t event;

    // parse the method
    if (!yaml_parser_parse(parser, &event))
        goto parse_request_error;

    if (0 == strncmp("get", (const char*)event.data.scalar.value, 3)) {
        req->method = HTTP_GET;
    } else if (0 == strncmp("post", (const char*)event.data.scalar.value, 4)) {
        req->method = HTTP_POST;
    } else {
        fprintf(stderr, "Unknown HTTP method '%s'\n", event.data.scalar.value);
        goto parse_request_error;
    }
    yaml_event_delete(&event);

    // parse the URL
    if (!yaml_parser_parse(parser, &event))
        goto parse_request_error;

    if (0 >= strlen((const char*)event.data.scalar.value)) {
        fprintf(stderr, "URL is too short\n");
        goto parse_request_error;
    }
    if (!(req->url = malloc(sizeof(char) * (strlen((const char*)event.data.scalar.value) + 1))))
        goto parse_request_error;

    strcpy(req->url, (const char*)event.data.scalar.value);
    yaml_event_delete(&event);

    // parse the payload or headers
    while (expected_ends > 0) {
        // printf("  expected_ends: %lu\n", expected_ends);
        if (!yaml_parser_parse(parser, &event))
            goto parse_request_error;

        switch (event.type) {
            case YAML_MAPPING_END_EVENT:
                // printf("  got MAPPING_END\n");
                expected_ends--;
                break;
            case YAML_MAPPING_START_EVENT:
                // printf("  got MAPPING_START\n");
                expected_ends++;
                break;

            case YAML_SCALAR_EVENT:
                if (0 == strncmp("payload", (const char*)event.data.scalar.value, 7)) {
                    // printf("  calling parse_payload()\n");
                    if (parse_payload(parser, &event, req))
                        goto parse_request_error;
                } else if (0 == strncmp("headers", (const char*)event.data.scalar.value, 7)) {
                    // make sure "headers:" is followed by a sequence
                    yaml_event_delete(&event);
                    if (!yaml_parser_parse(parser, &event))
                        goto parse_request_error;
                    if (event.type != YAML_SEQUENCE_START_EVENT)
                        goto parse_request_error;

                    // printf("  calling parse_headers()\n");
                    if (parse_headers(parser, req)) {
                        goto parse_request_error;
                    }
                } else {
                    fprintf(stderr, "Unknown request metadata '%s'\n", event.data.scalar.value);
                    goto parse_request_error;
                }
                break;

            default:
                fprintf(stderr, "unexpected YAML event: %d\n", event.type);
                goto parse_request_error;
        }
    }

    return req;

parse_request_error:
    // printf("in parse_request_error\n");
    if (req != NULL ) {
        if (req->url != NULL)
            free(req->url);
        if (req->payload != NULL)
            free(req->payload);
        if (req->headers != NULL) {
            for (i=0; i<req->num_headers; i++) {
                free(req->headers[i].name);
                free(req->headers[i].value);
            }
            curl_slist_free_all(req->curl_headers);
            free(req->headers);
        }
        free(req);
    }
    return NULL;
}


/**
 * Parse a URL file into a requests
 */
requests parse_urls(const char* url_filename)
{
    unsigned long i;

    requests reqs;
    request* req;

    list* req_list;
    node* n;
    FILE* urls;
    char done = 0;

    yaml_parser_t parser;
    yaml_event_t event;

    reqs.count = 0;
    reqs.reqs = NULL;
    req_list = list_new();

    urls = fopen(url_filename, "r");
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, urls);

    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
            break;
        } else {
            switch (event.type) {
                case YAML_MAPPING_START_EVENT:
                    // printf("event.type: MAPPING_START\n");
                    // printf("  calling parse_request()\n");
                    if ((req = parse_request(&parser)) == NULL) {
                        done = 2;
                    } else {
                        // printf("  got a good request!\n");
                        n = list_node_new(req);
                        list_push(req_list, n);
                    }
                    break;

                case YAML_STREAM_START_EVENT:
                case YAML_DOCUMENT_START_EVENT:
                case YAML_SEQUENCE_START_EVENT:
                case YAML_SEQUENCE_END_EVENT:
                    break;

                case YAML_STREAM_END_EVENT:
                case YAML_DOCUMENT_END_EVENT:
                    done = 1;
                    break;

                default:
                    // printf("got event type %d\n", event.type);
                    fprintf(stderr, "Malformed URLS_FILE at pos: %lu\n", (unsigned long)parser.offset);
                    done = 2;
            }
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    if (done == 2) {
        fprintf(stderr, "Error parsing URLs file\n");
        exit(1);
    }

    if (!(reqs.reqs = malloc(sizeof(request) * req_list->length))) {
        list_free(req_list, 1);
        exit(2);
    }

    n = req_list->head;
    for (i=0; i<req_list->length; i++) {
        req = (request *)(n->data);
        reqs.reqs[i].method = req->method;
        reqs.reqs[i].url = req->url;
        reqs.reqs[i].payload_length = req->payload_length;
        reqs.reqs[i].payload = req->payload;
        reqs.reqs[i].headers = req->headers;
        reqs.reqs[i].num_headers = req->num_headers;
        reqs.reqs[i].curl_headers = req->curl_headers;
        // printf("set request %lu with url %s\n", i, reqs.reqs[i].url);
        n = n->next;
    }
    reqs.count = req_list->length;
    list_free(req_list, 0);

    return reqs;
}
