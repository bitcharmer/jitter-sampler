#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <zconf.h>
#include "influx.h"

#define BATCH_SIZE 10
#define BUF_SIZE 2048

struct timespec sleep_requested = {.tv_sec = 0, .tv_nsec = 1000000l};
struct timespec sleep_remaining;

int sockfd;
char buf[BUF_SIZE];
char hostname[256];
struct sockaddr_in serverAddr;


void publish_batch(int idx, int batch_size, struct jitter* jitter, int64_t cpu) {
    int buf_offset = 0;
    for (int i = 0; i < batch_size; i++) {
        int64_t time = jitter[idx+i].timestamp;
        int64_t latency = jitter[idx+i].delay;
        buf_offset += sprintf((char*) (buf + buf_offset), "jitter,host=%s,cpu=%li latency=%li %li\n", hostname, cpu, latency, time);
    }

    sendto(sockfd, buf, strlen(buf), SOCK_NONBLOCK, &serverAddr, sizeof serverAddr);
    bzero(buf, BUF_SIZE);
    nanosleep(&sleep_requested, &sleep_remaining);
}

void init_udp(char *host, char *port) {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&serverAddr, 0, sizeof serverAddr);
    struct hostent *he;
    he = gethostbyname(host);
    memcpy(&serverAddr.sin_addr, he->h_addr_list[0], he->h_length);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(strtol(port, (char **)NULL, 10));
}

void process_out(int64_t size, struct jitter *results, int64_t cpu) {
    int idx = 0;
    while (idx + BATCH_SIZE < size) {
        publish_batch(idx, BATCH_SIZE, results, cpu);
        idx += BATCH_SIZE;
    }

    while (idx < size) {
        publish_batch(idx, 1, results, cpu);
        idx++;
    }
}

process_output init_influx(char* config_str) {
    char* host = strtok(config_str, ":");
    char* port = strtok(NULL, ":");
    printf("Writing results to influx: %s:%s\n", host, port);

    init_udp(host, port);
    gethostname(hostname, sizeof hostname);
    return &process_out;
}
