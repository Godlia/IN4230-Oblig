#include "mip_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void send_ping(const char *socket_path, const char *message, unsigned int dest_host)
{
    /*
     * TODO:
     * 1. Build a message like "PING:<message>".
     * 2. Send it to the MIP daemon over the UNIX socket.
     * 3. Wait up to 1 second for the matching "PONG:<message>" response.
     * 4. Measure elapsed time and print it.
     * 5. Print "timeout" on failure.
     */
    (void)socket_path;
    (void)message;
    (void)dest_host;

    printf("[ping_client] sending PING:%s to host %u\n", message, dest_host);
    printf("[ping_client] skeleton ready. Implement socket I/O and timeout logic.\n");
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
