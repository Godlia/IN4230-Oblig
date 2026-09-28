#include "mip_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void handle_message(const char *message)
{
    /*
     * TODO:
     * 1. Receive the incoming SDU from the daemon over the UNIX socket.
     * 2. Print the received text.
     * 3. Build a response like "PONG:<message>" and send it back.
     */
    printf("[ping_server] received: %s\n", message);
    printf("[ping_server] expected response: PONG:%s\n", message);
}

int main(int argc, char **argv)
{
    const char *socket_path = NULL;

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

    printf("[ping_server] socket lower: %s\n", socket_path);
    printf("[ping_server] skeleton ready. Implement the socket loop and reply logic.\n");

    /*
     * Real behaviour would open the UNIX socket, wait for incoming messages,
     * then call handle_message() for each payload.
     */
    handle_message("example-message");

    return 0;
}
