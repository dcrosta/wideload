#include <stdio.h>
#include <stdlib.h>
#include <argtable2.h>

#include "cli.h"

#define NOT_POSITIVE_INT(x) ((x)->count > 0 && (x)->ival[0] < 1)
#define CLI_ERR(msg) { fprintf(stderr, "%s\n", msg); exit(11); }

/**
 * Parse the command line options and return a struct options
 * after performing validation.
 */
options command_line_options(int argc, char* argv[])
{
    struct arg_lit* help = arg_lit0("h", "help", "Displays this help message");
    struct arg_lit* version = arg_lit0("v", "version", "Displays the version");
    struct arg_int* concurrency = arg_int0("c", "concurrency", "N", "Number of concurrent requests [1]");
    struct arg_int* run_seconds = arg_int0("s", "run-seconds", "N", "Length of test in seconds [10]");
    struct arg_int* run_requests = arg_int0("r", "run-requests", "N", "Number of requests to make");
    struct arg_int* fail_after = arg_int0("f", "fail-after", "N", "Number of milliseconds after which to consider requests failed");
    struct arg_int* fail_status = arg_int0("t", "fail-status", "N", "HTTP status code greater than which to consider requests failed [400]");
    struct arg_file* url_filename = arg_file0(NULL, NULL, "URL_FILE", "File of URLs to load test");
    struct arg_lit* randomize = arg_lit0(NULL, "randomize", "Start each thread at a random position in the URLs file [false]");
    struct arg_end* end = arg_end(20);

    options opts;

    void* argtable[] = {
        help,
        version,
        concurrency,
        run_seconds,
        run_requests,
        fail_after,
        fail_status,
        url_filename,
        randomize,
        end
    };

    if (arg_nullcheck(argtable) != 0) {
        fprintf(stderr, "Memory error parsing command line options\n");
        exit(10);
    }

    // check for -h/--help
    if (arg_parse(argc, argv, argtable) != 0) {
        arg_print_errors(stderr, end, "wideload");
        exit(10);
    }
    if (help->count > 0) {
        fprintf(stdout, "wideload");
        arg_print_syntax(stdout, argtable, "\n\n");

        fprintf(stdout, "OPTIONS:\n");
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");

        exit(0);
    } else if (version-> count > 0) {
        fprintf(stdout, "wideload %s\n", "0.1.0");
        exit(0);
    }


    // validate results
    if (run_seconds->count > 0 && run_requests->count > 0) {
        CLI_ERR("cannot specify boty -r/--run-requests and -s/--run-seconds");
    } else if (run_seconds->count == 0 && run_requests->count == 0) {
        run_seconds->count = 1;
        run_seconds->ival[0] = 30;
    }

    if (NOT_POSITIVE_INT(concurrency))
        CLI_ERR("-c/--concurrency must be a positive number");
    if (NOT_POSITIVE_INT(run_seconds))
        CLI_ERR("-s/--run-seconds must be a positive number");
    if (NOT_POSITIVE_INT(run_requests))
        CLI_ERR("-r/--run-requests must be a positive number");
    if (NOT_POSITIVE_INT(fail_after))
        CLI_ERR("-f/--fail-after must be a positive number");
    if (NOT_POSITIVE_INT(fail_status))
        CLI_ERR("-t/--fail-status must be a positive number");
    if (url_filename->count != 1)
        CLI_ERR("URL_FILE is required");

    opts.concurrency = (concurrency->count == 0 ? 1 : concurrency->ival[0]);
    opts.run_seconds = (run_seconds->count == 0 ? 0 : run_seconds->ival[0]);
    opts.run_requests = (run_requests->count == 0 ? 0 : run_requests->ival[0]);
    opts.fail_after = (fail_after->count == 0 ? 0 : fail_after->ival[0]);
    opts.fail_status = (fail_status->count == 0 ? 400 : fail_status->ival[0]);
    opts.url_filename = url_filename->filename[0];
    opts.randomize = (randomize->count > 0 ? 1 : 0);

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

    return opts;
}
