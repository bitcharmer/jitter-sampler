#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include <error.h>
#include <numa.h>
#include "jitter.h"
#include "influx.h"
#include "csv.h"

#define NANOS_IN_SEC 1000000000ul

struct timespec ts;

void print_args(int64_t duration, int64_t granularity, int64_t cpu);
void affinitze_to_cpu(int64_t cpu) ;
void parse_args(int argc, char *const *argv, int64_t *duration, int64_t *granularity, int64_t *cpu, process_output *process_output);


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


int64_t nano_time() {
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * NANOS_IN_SEC + ts.tv_nsec;
}


int64_t capture_jitter(int64_t duration, int64_t granularity, struct jitter *jitter) {
    int64_t ts = nano_time();
    int64_t deadline = ts + duration;
    int64_t next_report = ts + granularity;
    int64_t idx = 0, max = 0;

    while (ts < deadline) {
        int64_t now = nano_time();
        int64_t latency = now - ts;

        int64_t diff = max - latency;
        max -= diff & (diff >> 63);

        if (now > next_report) {
            jitter[idx++] = (struct jitter) {.timestamp = now, .delay = max};
            max = 0;
            next_report = now + granularity;
        }

        ts = now;
    }

    return idx;
}


int main(int argc, char* argv[]) {
    int64_t duration = 60 * NANOS_IN_SEC;
    int64_t granularity = NANOS_IN_SEC;
    int64_t cpu = 0;
    process_output process_output;

    parse_args(argc, argv, &duration, &granularity, &cpu, &process_output);
    print_args(duration, granularity, cpu);
    affinitze_to_cpu(cpu);

    struct jitter* jitter = numa_alloc_local(duration/granularity * sizeof(struct jitter));
    int64_t data_len = capture_jitter(duration, granularity, jitter);

    printf("Finished with %li results\n", data_len);
    process_output(data_len, jitter, cpu);

    return 0;
}


void parse_args(int argc, char *const *argv, int64_t *duration, int64_t *granularity, int64_t *cpu, process_output *process_output) {
    for (int idx = 1; idx < argc; idx++) {
        if (strcmp("-h", argv[idx]) == 0) print_usage();
        else if (strcmp("-c", argv[idx]) == 0) (*cpu) = strtol(argv[++idx], (char **)NULL, 10);
        else if (strcmp("-d", argv[idx]) == 0) (*duration) = strtol(argv[++idx], (char **)NULL, 10) * NANOS_IN_SEC;
        else if (strcmp("-r", argv[idx]) == 0) (*granularity) = strtol(argv[++idx], (char **)NULL, 10) * 1000000ul;
        else if (strcmp("-o", argv[idx]) == 0) {
            char *output = argv[++idx];
            if (strstr(output, "influx://")) (*process_output) = init_influx(output + 9);
            else if (strstr(output, "csv://")) (*process_output) = init_csv(output + 6);
        }
    }
}


void affinitze_to_cpu(int64_t cpu) {
    int64_t cpu_mask = 1 << cpu;
    int result = sched_setaffinity(0, sizeof(long long), (const cpu_set_t *) &cpu_mask);
    if (result != 0) {
        error(1, result, "Could not set affinity. Exiting...");
        exit(1);
    }
}


void print_args(int64_t duration, int64_t granularity, int64_t cpu) {
    printf("cpu: %li\n", cpu);
    printf("duration: %lus\n", duration/NANOS_IN_SEC);
    printf("report interval: %lims\n", granularity/1000000);
}

