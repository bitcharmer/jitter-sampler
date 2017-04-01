#ifndef JITTER_H
#define JITTER_H

struct jitter {
    unsigned long timestamp;
    unsigned long delay;
};

typedef void (*process_output) (unsigned long, struct jitter*);

#endif //JITTER_H
