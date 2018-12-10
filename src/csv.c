#include <stdio.h>
#include <stdlib.h>
#include "csv.h"

FILE *f;

void write_to_file(int64_t size, struct jitter *results, int64_t cpu) {
    for (int64_t i = 0; i < size; i++)
        fprintf(f, "%li,%li\n", results[i].timestamp, results[i].delay);

    fclose(f);
}

process_output init_csv(char* path) {
    f = fopen(path, "w");
    if (f == NULL) {
        printf("Error opening file\n");
        exit(1);
    }

    printf("Writing results to csv file: %s\n", path);
    fprintf(f, "time,delay\n");
    return &write_to_file;
}
