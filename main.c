#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include "cli.h"
#include "urlfile.h"
#include "loader.h"


int cmp_ul_asc(const void* aa, const void* bb)
{
    const unsigned long *a = aa, *b = bb;
    return (*a < *b) ? -1 : (*a > *b);
}

int main(int argc, char* argv[])
{
    unsigned long i, j, k;
    unsigned long failures = 0;
    unsigned long total = 0;
    FILE* csv;

    options opts = command_line_options(argc, argv);
    requests reqs = parse_urls(opts.url_filename);
    if (opts.randomize)
        srand(time(NULL));

    pthread_t threads[opts.concurrency];
    threadstate states[opts.concurrency];


    for (i=0; i<opts.concurrency; i++) {
        states[i].reqs = reqs.reqs;
        states[i].req_count = reqs.count;
        states[i].opts = opts;
        if (pthread_create(&threads[i], NULL, load_thread, &states[i]) != 0) {
            perror("thread error");
            exit(2);
        }
    }

    for (i=0; i<opts.concurrency; i++) {
        pthread_join(threads[i], NULL);
        pthread_detach(threads[i]);
    }

    // compute summary statistics
    csv = fopen("detailed-results.csv", "w");
    fprintf(csv, "method,url,time_start,time_first_byte,time_finish,status,bytes_received\n");

    for (i=0; i<opts.concurrency; i++) {
        threadstate state = states[i];
        for (j=0; j<state.rslt_count; j++) {
            result* rslt = &state.rslts[j];
            if (rslt->status >= opts.fail_status)
                failures++;
            else
                total++;
            fprintf(csv, "%s,%s,%.3f,%.3f,%.3f,%d,%lu\n",
                    rslt->req->method == HTTP_GET ? "GET" : "POST",
                    rslt->req->url,
                    rslt->time_start / 1000000.0,
                    rslt->time_first_byte / 1000000.0,
                    rslt->time_end / 1000000.0,
                    rslt->status,
                    rslt->num_bytes);
        }
    }

    unsigned long timings[total];

    k = 0;
    for (i=0; i<opts.concurrency; i++) {
        threadstate state = states[i];
        for (j=0; j<state.rslt_count; j++) {
            if (state.rslts[j].status >= opts.fail_status)
                continue;
            timings[k] = state.rslts[j].time_end - state.rslts[j].time_start;
            k++;
        }
    }

    qsort(timings, total, sizeof(unsigned long), cmp_ul_asc);

    printf("Request time (ms)\n");
    printf(" 50%%: %lu\n", (unsigned long)(timings[total / 2] / 1000.0));
    printf(" 75%%: %lu\n", (unsigned long)(timings[(unsigned long)(total * 0.75)] / 1000.0));
    printf(" 95%%: %lu\n", (unsigned long)(timings[(unsigned long)(total * 0.95)] / 1000.0));
    printf(" max: %lu\n", (unsigned long)(timings[total - 1] / 1000.0));
    printf("\n");

    printf("Failures: %lu\n", failures);

    for (i=0; i<opts.concurrency; i++) {
        threadstate state = states[i];
        free(state.rslts);
    }
    for (i=0; i<reqs.count; i++) {
        free(reqs.reqs[i].url);
        if (reqs.reqs[i].payload_length)
            free(reqs.reqs[i].payload);
    }
    free(reqs.reqs);

    return 0;
}
