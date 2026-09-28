#include "mip_common.h"

#include <stdio.h>

void print_mipd_usage(void)
{
    fprintf(stderr, "Usage: mipd [-h] [-d] <socket_upper> <MIP address>\n");
}

void print_ping_server_usage(void)
{
    fprintf(stderr, "Usage: ping_server [-h] <socket_lower>\n");
}

void print_ping_client_usage(void)
{
    fprintf(stderr, "Usage: ping_client [-h] <socket_lower> <message> <destination_host>\n");
}
