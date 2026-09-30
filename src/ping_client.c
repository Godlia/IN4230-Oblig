#include "mip_common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static void send_ping(const char *socket_path, const char *message, unsigned int dest_host)
{
    int fd;
    struct sockaddr_un addr;
    char request[256];
    char response[256];
    ssize_t sent;
    ssize_t received;
    struct timeval start_time;
    struct timeval end_time;
    fd_set read_fds;
    struct timeval timeout;
    char expected_response[256];
    char *payload = NULL;

    if (socket_path == NULL || message == NULL) {
        return;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return;
    }

    request[0] = (char)(unsigned char)dest_host;
    snprintf(request + 1, sizeof(request) - 1, "PING:%s", message);
    snprintf(expected_response, sizeof(expected_response), "PONG:%s", message);

    printf("[ping_client] sending PING:%s to host %u\n", message, dest_host);

    gettimeofday(&start_time, NULL);
    sent = send(fd, request, 1 + strlen(request + 1), 0);
    if (sent < 0) {
        perror("send");
        close(fd);
        return;
    }

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    if (select(fd + 1, &read_fds, NULL, NULL, &timeout) <= 0) {
        printf("[ping_client] timeout\n");
        close(fd);
        return;
    }

    received = recv(fd, response, sizeof(response) - 1, 0);
    if (received <= 0) {
        if (received < 0) {
            perror("recv");
        }
        printf("[ping_client] timeout\n");
        close(fd);
        return;
    }

    response[received] = '\0';
    gettimeofday(&end_time, NULL);

    payload = response + 1;
    if (response[0] != 0 && strcmp(payload, expected_response) == 0) {
        long elapsed_ms = 0;
        elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000L;
        elapsed_ms += (end_time.tv_usec - start_time.tv_usec) / 1000L;
        printf("[ping_client] response: %s\n", payload);
        printf("[ping_client] elapsed time: %ld ms\n", elapsed_ms);
    } else {
        printf("[ping_client] received unexpected response: %s\n", response);
        printf("[ping_client] timeout\n");
    }

    close(fd);
}

int main(int argc, char **argv)
{
    const char *socket_path = NULL;
    const char *message = NULL;
    unsigned int destination_host = 0;

    if (argc < 2) {
        print_ping_client_usage();
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0) {
            print_ping_client_usage();
            return 0;
        }

        if (socket_path == NULL) {
            socket_path = argv[i];
            continue;
        }

        if (message == NULL) {
            message = argv[i];
            continue;
        }

        if (destination_host == 0 && argv[i][0] != '\0') {
            destination_host = (unsigned int)strtoul(argv[i], NULL, 10);
            continue;
        }
    }

    if (socket_path == NULL || message == NULL || destination_host == 0) {
        print_ping_client_usage();
        return 1;
    }

    send_ping(socket_path, message, destination_host);
    return 0;
}
