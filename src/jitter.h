#ifndef JITTER_H
#define JITTER_H

#include <stdint.h>

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

struct jitter {
    long long timestamp;
    long long delay;
};

typedef void (*process_output) (long long, struct jitter*, int);

typedef long long (*time_func) (void);

struct program_args {
    _Bool disable_irqs;
    long long duration;
    long long granularity;
    int cpu;
    process_output process_output;
    time_func time_func;
};

#endif //JITTER_H
