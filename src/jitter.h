#ifndef JITTER_H
#define JITTER_H

struct jitter {
    unsigned long long timestamp;
    unsigned long long delay;
};

typedef void (*process_output) (unsigned long long, struct jitter*);

#endif //JITTER_H
