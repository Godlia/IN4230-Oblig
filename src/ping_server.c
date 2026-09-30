#include "mip_common.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static void handle_message(const char *message)
{
    if (message == NULL) {
        return;
    }

    printf("[ping_server] received: %s\n", message);
    printf("[ping_server] expected response: PONG:%s\n", message);
}

int main(int argc, char **argv)
{
    const char *socket_path = NULL;
    int fd = -1;
    struct sockaddr_un addr;

    if (argc < 2) {
        print_ping_server_usage();
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0) {
            print_ping_server_usage();
            return 0;
        }

        if (socket_path == NULL) {
            socket_path = argv[i];
        }
    }

    if (socket_path == NULL) {
        print_ping_server_usage();
        return 1;
    }

    /*
     * mipd owns and listens on socket_path; ping_server is only ever a
     * client of it (mipd is the long-running process, so it must be the
     * one binding the well-known path). We connect once and keep the
     * connection open for the lifetime of the process, since incoming
     * pings can arrive at any time and mipd delivers them by writing to
     * this same connection.
     */
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return 1;
    }

    printf("[ping_server] connected to mipd via %s\n", socket_path);
    printf("[ping_server] waiting for messages...\n");

    while (1) { //enter loop and listen for ping 
        char buffer[256];
        ssize_t received;
        char response[256];
        size_t response_len;

        received = recv(fd, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            if (received < 0) {
                perror("recv");
            } else {
                printf("[ping_server] mipd closed the connection\n");
            }
            break;
        }

        buffer[received] = '\0';
        //if something was received, get its adress and return a PONG
        if (received > 1) {
            uint8_t source_mip_address = (uint8_t)buffer[0];
            char *payload = buffer + 1;

            if (strncmp(payload, "PING:", 5) == 0) {
                char *message = payload + 5;
                handle_message(message);

                response[0] = (char)source_mip_address;
                response_len = (size_t)snprintf(response + 1, sizeof(response) - 1, "PONG:%s", message);
                if (response_len >= sizeof(response) - 1) {
                    response[sizeof(response) - 1] = '\0';
                    response_len = sizeof(response) - 1;
                }
                response_len += 1;

                printf("[ping_server] sending response: %s\n", response + 1);
                if (send(fd, response, response_len, 0) < 0) {
                    perror("send");
                }
            } else {
                printf("[ping_server] received unexpected message: %s\n", payload);
            }
        } else {
            printf("[ping_server] received unexpected message: %s\n", buffer);
        }
    }

    close(fd);
    return 0;
}
