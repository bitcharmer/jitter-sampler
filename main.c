#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sched.h>

#define BATCH_SIZE 10
#define BUF_SIZE 2048

struct timespec ts;
struct timespec sleep_requested;
struct timespec sleep_remaining;

long cpu_mask = -1l;
int sockfd;
char buf[BUF_SIZE];
struct sockaddr_in serverAddr;

void connect_udp(char *host, char *port);
int capture_jitter(unsigned long duration, unsigned long granularity, unsigned long *pInt);
void publish_results(int points, unsigned long *jitter);
void publish_batch(int idx, int batch_size, unsigned long *jitter);


long long nano_time() {
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000l + ts.tv_nsec;
}

int main(int argc, char* argv[]) {
    unsigned long duration = 60000000000ul;
    unsigned long granularity = 1000000000ul;
    char* influx_host;
    char* influx_port;

    int idx = 1;
    for (; idx < argc; idx++) {
        if (strcmp("-c", argv[idx]) == 0) cpu_mask = 1 << strtol(argv[++idx], (char **)NULL, 10);
        if (strcmp("-d", argv[idx]) == 0) duration = strtol(argv[++idx], (char **)NULL, 10) * 1000000000ul;
        if (strcmp("-r", argv[idx]) == 0) granularity = strtol(argv[++idx], (char **)NULL, 10) * 1000000ul;
        if (strcmp("-i", argv[idx]) == 0) {
            influx_host = strtok(argv[++idx], ":");
            influx_port = strtok(NULL, ":");
            connect_udp(influx_host, influx_port);
        }
    }

    printf("duration: %lu\n", duration);
    printf("report granularity: %lu\n", granularity);
    printf("host: %s\n", influx_host);
    printf("port: %s\n", influx_port);

    unsigned long* jitter = calloc(duration/granularity, sizeof(unsigned long)*2);
    long data_points = capture_jitter(duration, granularity, jitter);
    publish_results(data_points, jitter);

    return 0;
}

void connect_udp(char *host, char *port) {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&serverAddr, 0, sizeof(serverAddr));
    struct hostent *he;
    he = gethostbyname(host);
    memcpy(&serverAddr.sin_addr, he->h_addr_list[0], he->h_length);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(strtol(port, (char **)NULL, 10));
}

void publish_results(int points, unsigned long *jitter) {
    int idx = 0;
    while (idx + BATCH_SIZE < points) {
        publish_batch(idx, BATCH_SIZE, jitter);
        idx += BATCH_SIZE;
    }

    while (idx < points) {
        publish_batch(idx, 1, jitter);
        idx++;
    }
}

void publish_batch(int idx, int batch_size, unsigned long *jitter) {
    int buf_offset = 0;
    for (int i = 0; i < batch_size; i++) {
        unsigned long time = jitter[(idx+i)*2];
        unsigned long latency = jitter[(idx+i)*2 + 1];
        buf_offset += sprintf((char*) (buf + buf_offset), "jitter,host=qdlt-xps latency=%lu %lu\n", latency, time);
    }

    sendto(sockfd, buf, strlen(buf), SOCK_NONBLOCK, &serverAddr, sizeof(serverAddr));
    bzero(buf, BUF_SIZE);
    nanosleep(&sleep_requested, &sleep_remaining);
}

int capture_jitter(unsigned long duration, unsigned long granularity, unsigned long *jitter) {
    long long int ts = nano_time();
    unsigned long deadline = ts + duration;
    unsigned long next_report = ts + granularity;
    unsigned long max = 0;
    int idx = 0;

    sleep_requested.tv_sec = 0;
    sleep_requested.tv_nsec = 1000000l;

    sched_setaffinity(0, sizeof(long), (const cpu_set_t *) &cpu_mask);

//    while (ts < deadline) {
//        long long int now = nano_time();
//        long long int latency = now - ts;
//
//        if (latency > max) max = latency;
//        if (now > next_report) {
//            jitter[0] = now;
//            jitter[1] = max;
//            max = 0;
//            publish_batch(0, 1, jitter);
//            now = nano_time();
//            next_report = now + granularity;
//        }
//
//        ts = now;
//    }
    while (ts < deadline) {
        long long int now = nano_time();
        long long int latency = now - ts;

        if (latency > max) max = latency;
        if (now > next_report) {
            jitter[idx++] = now;
            jitter[idx++] = max;
            max = 0;
            next_report = now + granularity;
        }

        ts = now;
    }

    return idx/2;
}