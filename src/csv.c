#include <stdio.h>
#include <stdlib.h>
#include "csv.h"

FILE *f;

void write_to_file(long long size, struct jitter *results, int cpu) {
    for (long long i = 0; i < size; i++)
        fprintf(f, "%lli,%lli\n", results[i].timestamp, results[i].delay);

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