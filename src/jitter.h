#ifndef JITTER_H
#define JITTER_H

#include <stdint.h>

typedef void (*process_output) (int64_t, struct jitter*, int64_t);
typedef int64_t (*time_func) (void);

struct jitter {
    int64_t timestamp;
    int64_t delay;
};

struct program_args {
    _Bool hist_mode;
    _Bool disable_irqs;
    int64_t duration;
    int64_t granularity;
    int cpu;
    process_output process_output;
    time_func time_func;
};

#endif //JITTER_H
