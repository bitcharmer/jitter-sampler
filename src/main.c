#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include "jitter.h"
#include "influx.h"

struct timespec ts;
long cpu_mask = -1l;

unsigned long nano_time() {
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000l + ts.tv_nsec;
}

unsigned long capture_jitter(unsigned long duration, unsigned long granularity, struct jitter *jitter) {
    unsigned long ts = nano_time();
    unsigned long deadline = ts + duration;
    unsigned long next_report = ts + granularity;
    unsigned long max = 0;
    unsigned long idx = 0;

    sched_setaffinity(0, sizeof(long), (const cpu_set_t *) &cpu_mask);

    while (ts < deadline) {
        unsigned long now = nano_time();
        unsigned long latency = now - ts;

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
    unsigned long duration = 60000000000ul;
    unsigned long granularity = 1000000000ul;
    process_output out_function;

    int idx = 1;
    for (; idx < argc; idx++) {
        if (strcmp("-c", argv[idx]) == 0) cpu_mask = 1 << strtol(argv[++idx], (char **)NULL, 10);
        if (strcmp("-d", argv[idx]) == 0) duration = strtol(argv[++idx], (char **)NULL, 10) * 1000000000ul;
        if (strcmp("-r", argv[idx]) == 0) granularity = strtol(argv[++idx], (char **)NULL, 10) * 1000000ul;
        if (strcmp("-o", argv[idx]) == 0) {
            char *output = argv[++idx];
            if (strstr(output, "influx://")) out_function = init_influx(output+9);
            // add csv
        }
    }

    printf("duration: %lu\n", duration);
    printf("report granularity: %lu\n", granularity);

    struct jitter* jitter = calloc(duration/granularity, sizeof(struct jitter));
    long data_points = capture_jitter(duration, granularity, jitter);
    out_function(data_points, jitter);

    return 0;
}


