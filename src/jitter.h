#ifndef JITTER_H
#define JITTER_H

#include <stdint.h>

struct jitter {
    int64_t timestamp;
    int64_t delay;
};

typedef void (*process_output) (int64_t, struct jitter*, int64_t);

#endif //JITTER_H
