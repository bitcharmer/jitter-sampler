#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <zconf.h>
#include <error.h>
#include <errno.h>
#include "influx.h"

#define BATCH_SIZE 10
#define BUF_SIZE 1500

static struct timespec sleep_requested = {.tv_sec = 0, .tv_nsec = 2000000l};
struct timespec sleep_remaining;

int sockfd;
char buf[BUF_SIZE];
char hostname[256];
struct sockaddr_in serverAddr;


void publish() {
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

void process_out(long long size, struct jitter *results, int cpu) {
    char line[200];
    int offset = 0;

    for (int i = 0; i < size; i++) {
        int len = sprintf(line, "jitter,host=%s,cpu=%i latency=%lli %lli\n", hostname, cpu, results[i].delay, results[i].timestamp);
        if (offset + len >= BUF_SIZE) {
            publish();
            offset = 0;
            bzero(buf, BUF_SIZE);
        }
        strcat(buf, line);
        offset += len;
    }

    if (offset > 0) publish();
}

process_output init_influx(char* config_str) {
    char* host = strtok(config_str, ":");
    char* port = strtok(NULL, ":");
    printf("Writing results to influx: %s:%s\n", host, port);

    init_udp(host, port);
    int err = gethostname(hostname, sizeof hostname);
    if (err) {
        error(1, errno, "Unable to resolve hostname: %s\n", host);
    }

    return &process_out;
}
