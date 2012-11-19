#ifndef WIDELOAD_CLI_H
#define WIDELOAD_CLI_H

#include <stdio.h>

typedef struct {
    /* required arguments */
    const char*    url_filename;

    /* optional arguments */
    unsigned long  concurrency;
    unsigned long  run_seconds;
    unsigned long  run_requests;
    unsigned long  fail_after;
    unsigned long  fail_status;
} options;

/**
 * Parse the command line options and return a struct options
 * after performing validation.
 */
options command_line_options(int argc, char* argv[]);

#endif
