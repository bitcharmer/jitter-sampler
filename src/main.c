#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include "jitter.h"
#include "influx.h"
#include "csv.h"

#define NANOS_IN_SEC 1000000000ul

struct timespec ts;
long long cpu_mask = -1l;


void print_usage() {
    puts("Usage:\n");
    puts("-h\tdisplay this usage message");
    puts("-c\ttarget cpu to run on (default: any)");
    puts("-d\tsampling duration in seconds (default: 60)");
    puts("-r\tjitter reporting interval in milliseconds (default: 1000)");
    puts("-o\toutput results using an output plugin. Supported plugins:");
    puts("\tinflux://<host:port>\tstores results in influxDb with line protocol over UDP");
    puts("\tcsv://<file>\tstores results in a csv file");

    exit(0);
}

unsigned long long nano_time() {
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * NANOS_IN_SEC + ts.tv_nsec;
}

unsigned long long capture_jitter(unsigned long long duration, unsigned long long granularity, struct jitter *jitter) {
    unsigned long long ts = nano_time();
    unsigned long long deadline = ts + duration;
    unsigned long long next_report = ts + granularity;
    unsigned long long max = 0;
    unsigned long long idx = 0;

    sched_setaffinity(0, sizeof(long long), (const cpu_set_t *) &cpu_mask);

    while (ts < deadline) {
        unsigned long long now = nano_time();
        unsigned long long latency = now - ts;

        if (latency > max) max = latency;
        if (now > next_report) {
            jitter[idx++] = (struct jitter) {.timestamp = now, .delay = max};
            max = 0;
            next_report = now + granularity;
        }

        ts = now;
    }

    return idx >> 1L;
}

int main(int argc, char* argv[]) {
    unsigned long long duration = 60 * NANOS_IN_SEC;
    unsigned long long granularity = NANOS_IN_SEC;
    process_output out_function;

    int idx = 1;
    for (; idx < argc; idx++) {
        if (strcmp("-h", argv[idx]) == 0) print_usage();
        else if (strcmp("-c", argv[idx]) == 0) cpu_mask = 1 << strtol(argv[++idx], (char **)NULL, 10);
        else if (strcmp("-d", argv[idx]) == 0) duration = strtol(argv[++idx], (char **)NULL, 10) * NANOS_IN_SEC;
        else if (strcmp("-r", argv[idx]) == 0) granularity = strtol(argv[++idx], (char **)NULL, 10) * 1000000ul;
        else if (strcmp("-o", argv[idx]) == 0) {
            char *output = argv[++idx];
            if (strstr(output, "influx://")) out_function = init_influx(output+9);
            else if (strstr(output, "csv://")) out_function = init_csv(output+6);
        }
    }

    printf("duration: %llus\n", duration/NANOS_IN_SEC);
    printf("report granularity: %llums\n", granularity/1000000);

    struct jitter* jitter = calloc(duration/granularity, sizeof(struct jitter));
    long long data_points = capture_jitter(duration, granularity, jitter);
    out_function(data_points, jitter);

    return 0;
}

