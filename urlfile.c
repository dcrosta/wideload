#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "urlfile.h"
#include "list.h"
#include "base64decode.h"


/**
 * Parse a URL file line into a request
 */
request parse_line(char* line, const char* url_filename, int lineno)
{
    request req;
    char* method = NULL;
    char* url = NULL;
    char* payload = NULL;
    char* tmp = line;

    // split line by tabs into method, url, payload
    method = strsep(&tmp, "\t");
    url = strsep(&tmp, "\t\n\0");
    payload = strsep(&tmp, "\n\0");

    if (method == NULL || url == NULL) {
        fprintf(stderr, "%s line %d: missing method or URL\n", url_filename, lineno);
        exit(5);
    }

    if (strcmp(method, "GET") == 0) {
        req.method = HTTP_GET;
    } else if (strcmp(method, "POST") == 0) {
        req.method = HTTP_POST;
    } else {
        fprintf(stderr, "%s line %d: unknown method '%s'\n", url_filename, lineno, method);
        exit(5);
    }

    req.url = malloc(sizeof(char) * (strlen(url) + 1));
    strcpy(req.url, url);

    if (payload != NULL) {
        if (req.method == HTTP_GET) {
            fprintf(stderr, "%s line %d: GET requests should not have a payload", url_filename, lineno);
            exit(5);
        }

        // validate base64-encoded payload data
        if (strlen(payload) % 4 != 0) {
            fprintf(stderr, "%s line %d: POST payload length must be multiple of 4 (was %d)", url_filename, lineno, (int)strlen(payload));
            exit(5);
        }

        unsigned long rawlength = strlen(payload);
        req.payload_length = rawlength / 4 * 3;
        if (payload[rawlength - 1] == '=')
            req.payload_length--;
        if (payload[rawlength - 2] == '=')
            req.payload_length--;

        req.payload = malloc(sizeof(unsigned char) * (req.payload_length + 1));
        req.payload[req.payload_length] = '\0';

        base64_decodestate state;
        base64_init_decodestate(&state);
        base64_decode_block(payload, rawlength, req.payload, &state);
    } else {
        req.payload_length = 0;
        req.payload = NULL;
    }

    return req;
}

/**
 * Parse a URL file into a requests
 */
requests parse_urls(const char* url_filename)
{
    requests out;

    list* lines;
    node* node;
    struct stat url_file_stat;
    FILE* urls;
    off_t length;
    off_t offset = 0;

    unsigned long i = 0;
    unsigned long amt;
    unsigned long len;

    char buf[1048577];
    char * pos;
    char * line;

    urls = fopen(url_filename, "r");

    stat(url_filename, &url_file_stat);
    length = url_file_stat.st_size;

    lines = list_new();

    while (offset < length) {
        fseek(urls, offset, 0);
        amt = fread(buf, sizeof(char), 1048576, urls);
        buf[amt+1] = '\0';

        pos = strchr(buf, '\n');
        if (pos == NULL) {
            if (feof(urls)) {
                pos = buf + amt;
            } else {
                fprintf(stderr, "%s line %ld: line is too long (> 1MB)\n", url_filename, i);
                exit(5);
            }
        }

        len = pos - buf;
        offset += len + 1;

        line = malloc(sizeof(char) * (len + 1));
        strncpy(line, buf, len);
        line[len] = '\0';

        node = list_node_new(line);
        list_push(lines, node);

        i++;
    }

    out.count = lines->length;
    out.reqs = malloc(sizeof(request) * lines->length);

    node = lines->head;
    i = 0;
    while (node != NULL) {
        out.reqs[i] = parse_line((char*)node->data, url_filename, i);
        node = node->next;
        i++;
    }

    list_free(lines, 1);
    return out;
}



