#include <stdio.h>
#include "jitter.h"
#include "influx.h"
#include "csv.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include <error.h>
#include <errno.h>
#include <numa.h>
#include <sys/mman.h>
#include <sys/io.h>

#define NANOS_IN_SEC 1000000000ul

long double cpu_freq = 0;
unsigned long long time_offset;

void print_args(struct program_args *args);
void affinitze_to_cpu(unsigned int cpu) ;
void parse_args(int argc, char *const *argv, struct program_args *args);


void print_usage() {
    puts("Usage:\n");
    puts("-h\tdisplay this usage message");
    puts("-c\ttarget cpu to run on (default: any)");
    puts("-t\ttimestamp capturing method (default: realtime). Allowed values:"
         "\n\trealtime (uses clock_gettime(CLOCK_REALTIME),"
         "\n\trdtsc (uses RDTSC instruction, requires passing target CPU frequency with -f argument");
    puts("-f\target cpu frequency in GHz as decimal number");
    puts("-d\tsampling duration in seconds (default: 60)");
    puts("-r\tjitter reporting interval in milliseconds (default: 1000)");
    puts("-o\toutput results using an output plugin. Supported plugins:");
    puts("\tinflux://<host:port>\tstores results in influxDb with line protocol over UDP");
    puts("\tcsv://<file>\tstores results in a csv file");

    exit(0);
}


static __inline__ int64_t rdtsc(void) {
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return (( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 ));
}

static __inline__ int64_t rdtscp_time(void)
{
    unsigned long long tsc;

    __asm__ __volatile__(
    "rdtscp;"
    "shl $32, %%rdx;"
    "or %%rdx, %%rax"
    : "=a"(tsc)
    :
    : "%rcx", "%rdx");

    return tsc / cpu_freq;
}

static __inline__ int64_t rdtsc_time(void) {
        return rdtsc() / cpu_freq + time_offset;
}

static inline int64_t nano_time() {
    static struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * NANOS_IN_SEC + ts.tv_nsec;
}

int64_t capture_jitter(int64_t duration, int64_t granularity, struct jitter *jitter, time_func time_func) {
    int64_t ts = time_func();
    int64_t deadline = ts + duration;
    int64_t next_report = ts + granularity;
    int64_t idx = 0, max = 0;

    while (ts < deadline) {
        int64_t now = time_func();
        int64_t latency = now - ts;

        if (latency > max) max = latency;

        if (now > next_report) {
            jitter[idx++] = (struct jitter) {.timestamp = now, .delay = max};
            max = 0;
            now = time_func();
            next_report = now + granularity;
        }

        ts = now;
    }

    return idx;
}


int main(int argc, char* argv[]) {
    struct program_args args = {
            .hist_mode = 0,
            .disable_irqs = 0,
            .duration = 60 * NANOS_IN_SEC,
            .granularity = NANOS_IN_SEC,
            .cpu = -1,
            .process_output = NULL,
            .time_func = nano_time
    };

    parse_args(argc, argv, &args);
    print_args(&args);

    if (args.cpu >= 0) affinitze_to_cpu(args.cpu);

    int ret = mlockall(MCL_CURRENT | MCL_FUTURE);
    if (ret) {
        puts("Warning: mlockall() failed. This may impact the results");
    }

    int64_t items = args.duration / args.granularity;
    size_t sz = items * sizeof(struct jitter);
    printf("Mmapping %lu KB for data\n", sz / 1024);
    struct jitter *jitter = mmap(NULL, sz, PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE | MAP_LOCKED, 0, 0);

    if (jitter == MAP_FAILED) {
        error(1, errno, "Mmapping failed");
    }

    madvise(jitter, sz, MADV_SEQUENTIAL);

    if (args.disable_irqs && args.cpu >= 0) {
        printf("Disabling IRQs on cpu %i (this may require root)", args.cpu);
        int err = iopl(3);
        if (err)
            error(1, errno, "Error while changing privilege level with iopl()");

        asm("cli");
    }

    if (args.time_func == rdtsc_time) {
        if (cpu_freq == 0)
            error(1, 0, "Using RDTSC-based time requires passing cpu frequency with -f argument");

        int64_t now = nano_time();
        time_offset = now - rdtscp_time();
    }

    // code/data warm-up
    capture_jitter(1000000000L, 1000000L, jitter, args.time_func);

    puts("Running jitter measurement");
    int64_t data_len = capture_jitter(args.duration, args.granularity, jitter, args.time_func);

    if (args.disable_irqs && args.cpu >= 0) {
        // re-enable IRQs
        asm("sti");
    }

    printf("Finished measurement. Captured %li data points\n", data_len);
    args.process_output(data_len, jitter, args.cpu);
    munmap(jitter, sz);
}

void parse_args(int argc, char *const *argv, struct program_args *args) {
    for (int idx = 1; idx < argc; idx++) {
        if (strcmp("-h", argv[idx]) == 0) print_usage();
        else if (strcmp("-histo", argv[idx]) == 0) args->hist_mode = 1;
        else if (strcmp("-c", argv[idx]) == 0) args->cpu = strtol(argv[++idx], (char **)NULL, 10);
        else if (strcmp("-d", argv[idx]) == 0) args->duration = strtol(argv[++idx], (char **)NULL, 10) * NANOS_IN_SEC;
        else if (strcmp("-r", argv[idx]) == 0) args->granularity = strtol(argv[++idx], (char **)NULL, 10) * 1000000ul;
        else if (strcmp("-o", argv[idx]) == 0) {
            char *output = argv[++idx];
            if (strstr(output, "influx://")) args->process_output = init_influx(output + 9);
            else if (strstr(output, "csv://")) args->process_output = init_csv(output + 6);
        }
        else if (strcmp("-t", argv[idx]) == 0) {
            char *time_method = argv[++idx];
            if (strstr(time_method, "rdtsc")) args->time_func = rdtsc_time;
        }
        else if (strcmp("-f", argv[idx]) == 0) {
            char *freq_str = argv[++idx];
            cpu_freq = strtold(freq_str, NULL);
        }
        else if (strcmp("-noirqs", argv[idx]) == 0) args->disable_irqs = 1;
    }
}


void affinitze_to_cpu(unsigned int cpu) {
    int64_t cpu_mask = 1LLU << cpu;
    int result = sched_setaffinity(0, sizeof(long long), (const cpu_set_t *) &cpu_mask);
    if (result != 0) {
        error(1, errno, "Could not pin to cpu %i. Exiting...", cpu);
    }
}


void print_args(struct program_args *args) {
    int cpu = args->cpu;
    printf("Target cpu: ");
    if (cpu >= 0) printf("%i\n", args->cpu);
    else puts("not pinned");
    
    printf("Duration: %lus\n", args->duration/NANOS_IN_SEC);
    printf("Report interval: %lims\n", args->granularity/1000000);
}