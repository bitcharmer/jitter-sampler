#include <stdio.h>
#include <unistd.h>
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

void affinitze_to_cpu(unsigned int cpu) ;
void print_usage();
void parse_args(int argc, char *const *argv, struct program_args *args);
void print_args(struct program_args *args);


static __inline__ long long rdtsc(void) {
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return (( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 ));
}


static __inline__ long long rdtscp_time(void) {
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


static __inline__ long long rdtsc_time(void) {
        return rdtsc() / cpu_freq + time_offset;
}


static inline long long nano_time() {
    static struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * NANOS_IN_SEC + ts.tv_nsec;
}


long long capture_jitter(long long duration, long long granularity, struct jitter *jitter, time_func time_func) {
    long long ts = time_func();
    long long deadline = ts + duration;
    long long next_report = ts + granularity;
    long long idx = 0, max = 0;

    while (ts < deadline) {
        long long now = time_func();
        long long latency = now - ts;

        ts = now;

        if (latency > max) max = latency;
        if (now > next_report) {
            jitter[idx++] = (struct jitter) {.timestamp = now, .delay = max};
            max = 0;
            now = time_func();
            next_report = now + granularity;
        }
    }

    return idx;
}


void affinitze_to_cpu(unsigned int cpu) {
    long long cpu_mask = 1LLU << cpu;
    int result = sched_setaffinity(0, sizeof(long long), (const cpu_set_t *) &cpu_mask);
    if (result != 0) {
        error(1, errno, "Could not pin to cpu %i. Exiting...", cpu);
    }
}


int main(int argc, char* argv[]) {
    struct program_args args = {
            .disable_irqs = 0,
            .duration = 60 * NANOS_IN_SEC,
            .granularity = NANOS_IN_SEC,
            .cpu = -1,
            .process_output = NULL,
            .time_func = nano_time
    };

    if (argc == 1) {
        print_usage();
        exit(0);
    }

    parse_args(argc, argv, &args);
    print_args(&args);

    if (args.cpu >= 0) affinitze_to_cpu(args.cpu);

    int ret = mlockall(MCL_CURRENT | MCL_FUTURE);
    if (ret) {
        puts("Warning: mlockall() failed. This may impact the results");
    }

    long long items = args.duration / args.granularity;
    size_t sz = items * sizeof(struct jitter);
    printf("Mmapping %lu KB for data\n", sz / 1024);
    struct jitter *jitter = mmap(NULL, sz, PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE | MAP_LOCKED, 0, 0);

    if (jitter == MAP_FAILED) {
        error(1, errno, "Mmapping failed");
    }

    madvise(jitter, sz, MADV_SEQUENTIAL);

    if (args.disable_irqs && args.cpu >= 0) {
        printf("Disabling IRQs on cpu %i (this may require root)\n", args.cpu);
        int err = iopl(3);
        if (err)
            error(1, errno, "Error while changing privilege level with iopl()");

        asm volatile("cli": : :"memory");
    }

    if (args.time_func == rdtsc_time) {
        if (cpu_freq == 0)
            error(1, 0, "Using RDTSC-based time requires passing cpu frequency with -f argument");

        long long now = nano_time();
        time_offset = now - rdtscp_time();
    }

    // code/data warm-up
    capture_jitter(1000000000L, 1000000L, jitter, args.time_func);

    puts("Running jitter measurement");
    long long data_len = capture_jitter(args.duration, args.granularity, jitter, args.time_func);

    if (args.disable_irqs && args.cpu >= 0) {
        // re-enable IRQs
        asm("sti");
    }

    printf("Finished measurement. Captured %lli data points\n", data_len);
    args.process_output(data_len, jitter, args.cpu);
    munmap(jitter, sz);
}


void print_usage() {
    puts("Usage:\n");
    puts(" -h             display this usage message");
    puts(" -n             disable local interrupts on the target cpu for the duration of the program run (may require sudo)");
    puts(" -c cpu         target cpu to run on (default: any)");
    puts(" -t method      timestamp capturing method (default: realtime). Allowed values:\n"
         "                - realtime  (uses clock_gettime(CLOCK_REALTIME),\n"
         "                - rdtsc     (uses RDTSC instruction, requires passing target CPU frequency with -f argument\n");
    puts(" -f frequency   target cpu frequency in GHz as decimal number");
    puts(" -d duration    sampling duration in seconds (default: 60)");
    puts(" -r interval    jitter reporting interval in milliseconds (default: 1000)");
    puts(" -o output      output results using an output plugin. Supported plugins:");
    puts("\tinflux://<host:port>\tstores results in influxDb with line protocol over UDP");
    puts("\tcsv://<file>\tstores results in a csv file");

    exit(0);
}


void parse_args(int argc, char *const *argv, struct program_args *args) {
    int opt;

    while ((opt = getopt(argc, argv, "+hnc:d:r:o:t:f:")) != -1) {
        switch (opt) {
            case 'h':
                print_usage();
                return;
            case 'c':
                args->cpu = strtol(optarg, (char **)NULL, 10);
                break;
            case 'd':
                args->duration = strtol(optarg, (char **)NULL, 10) * NANOS_IN_SEC;
                break;
            case 'r':
                args->granularity = strtol(optarg, (char **)NULL, 10) * 1000000ul;
                break;
            case 'o':
                if (strstr(optarg, "influx://")) args->process_output = init_influx(optarg + 9);
                else if (strstr(optarg, "csv://")) args->process_output = init_csv(optarg + 6);
                break;
            case 't':
                if (strstr(optarg, "rdtsc")) args->time_func = rdtsc_time;
                break;
            case 'f':
                cpu_freq = strtold(optarg, NULL);
                break;
            case 'n':
                args->disable_irqs = 1;
                break;
            default:
                print_usage();
                exit(EXIT_FAILURE);
        }
    }
}

void print_args(struct program_args *args) {
    int cpu = args->cpu;
    printf("Target cpu: ");
    if (cpu >= 0) printf("%i\n", args->cpu);
    else puts("not pinned");
    
    printf("Duration: %llis\n", args->duration/NANOS_IN_SEC);
    printf("Report interval: %llims\n", args->granularity/1000000);
}