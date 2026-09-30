#include "mip.h"
#include "mip_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/*
 * Global ARP cache for the daemon.
 *
 * This is intentionally tiny and only acts as a skeleton for the assignment.
 * A real implementation would keep a fresh mapping between MIP addresses and
 * MAC addresses and would refresh or discard old entries.
 */
static mip_arp_entry_t arp_cache[MIP_MAX_ARP_CACHE];

static void init_arp_cache(void)
{
    for (unsigned int i = 0; i < MIP_MAX_ARP_CACHE; ++i) {
        arp_cache[i].mip_address = 0;
        memset(arp_cache[i].mac_address, 0, sizeof(arp_cache[i].mac_address));
        arp_cache[i].valid = 0;
    }
}

static void handle_upper_layer_message(const char *socket_path, uint8_t source_mip_address)
{
    const uint8_t destination_mip_address = 1u;
    const char payload[] = "HELLO";
    uint8_t frame[sizeof(mip_header_t) + sizeof(payload) - 1u];
    mip_header_t *header = (mip_header_t *)frame;

    memset(frame, 0, sizeof(frame));

    header->destination = destination_mip_address;
    header->source = source_mip_address;
    header->ttl = 64u;
    header->sdu_length = (uint16_t)(sizeof(payload) - 1u);
    header->sdu_type = 1u;
    memcpy(frame + sizeof(mip_header_t), payload, sizeof(payload) - 1u);

    printf("[mipd] handle_upper_layer_message(): socket=%s source=%u dest=%u payload=%s\n",
           socket_path != NULL ? socket_path : "(null)",
           source_mip_address,
           destination_mip_address,
           payload);
    printf("  built MIP PDU: ttl=%u type=%u length=%u\n",
           header->ttl,
           header->sdu_type,
           header->sdu_length);

    /*
     * The full implementation later will:
     *  - read the raw message from the UNIX socket
     *  - extract the destination MIP address from the payload
     *  - find the destination MAC in the ARP cache
     *  - encapsulate the SDU in a MIP packet and send it via the Ethernet socket
     */
}

static void handle_incoming_mip_packet(const uint8_t *frame, size_t frame_length)
{
    if (frame == NULL || frame_length < sizeof(mip_header_t)) {
        printf("[mipd] invalid MIP packet: frame is NULL or too short\n");
        return;
    }

    const mip_header_t *header = (const mip_header_t *)frame;
    const uint8_t *payload = frame + sizeof(mip_header_t);
    size_t payload_length = frame_length - sizeof(mip_header_t);

    printf("[mipd] incoming MIP packet:\n");
    printf("  dst=%u src=%u ttl=%u sdu_type=%u sdu_length=%u\n",
           header->destination,
           header->source,
           header->ttl,
           header->sdu_type,
           header->sdu_length);

    if (payload_length > 0) {
        printf("  payload:");
        for (size_t i = 0; i < payload_length; ++i) {
            printf(" %02X", payload[i]);
        }
        printf("\n");
    }

    /*
     * Later, this function should also:
     *  - verify the packet is for this host or broadcast
     *  - check whether it is ordinary data or a MIP-ARP packet
     *  - pass the payload to the upper layer or respond with ARP
     */
}

static void learn_arp_mapping(uint8_t mip_address, const uint8_t *mac_address)
{
    if (mac_address == NULL) {
        return;
    }

    for (unsigned int i = 0; i < MIP_MAX_ARP_CACHE; ++i) {
        if (arp_cache[i].valid && arp_cache[i].mip_address == mip_address) {
            memcpy(arp_cache[i].mac_address, mac_address, sizeof(arp_cache[i].mac_address));
            return;
        }
    }

    for (unsigned int i = 0; i < MIP_MAX_ARP_CACHE; ++i) {
        if (!arp_cache[i].valid) {
            arp_cache[i].mip_address = mip_address;
            memcpy(arp_cache[i].mac_address, mac_address, sizeof(arp_cache[i].mac_address));
            arp_cache[i].valid = 1;
            return;
        }
    }

    /*
     * No free slot exists, so replace the first entry as a simple fallback.
     * A more advanced implementation could evict the oldest or least-recently
     * used mapping instead.
     */
    arp_cache[0].mip_address = mip_address;
    memcpy(arp_cache[0].mac_address, mac_address, sizeof(arp_cache[0].mac_address));
    arp_cache[0].valid = 1;
}

static void print_debug_state(void)
{
    printf("[mipd] ARP cache:\n");

    for (unsigned int i = 0; i < MIP_MAX_ARP_CACHE; ++i) {
        if (arp_cache[i].valid) {
            printf("  [%u] mip=%u mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
                   i,
                   arp_cache[i].mip_address,
                   arp_cache[i].mac_address[0],
                   arp_cache[i].mac_address[1],
                   arp_cache[i].mac_address[2],
                   arp_cache[i].mac_address[3],
                   arp_cache[i].mac_address[4],
                   arp_cache[i].mac_address[5]);
        } else {
            printf("  [%u] empty\n", i);
        }
    }
}

static int create_unix_socket(const char *socket_path)
{
    int fd;
    struct sockaddr_un addr;
    if (socket_path == NULL) {
        return -1;
    }

    unlink(socket_path);

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 5) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

int main(int argc, char **argv)
{
    const char *socket_path = NULL;
    unsigned int mip_address = 0;
    int debug_mode = 0;

    if (argc < 2) {
        print_mipd_usage();
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0) {
            print_mipd_usage();
            return 0;
        }

        if (strcmp(argv[i], "-d") == 0) {
            debug_mode = 1;
            continue;
        }

        if (socket_path == NULL) {
            socket_path = argv[i];
            continue;
        }

        if (mip_address == 0 && argv[i][0] != '\0') {
            mip_address = (unsigned int)strtoul(argv[i], NULL, 10);
            continue;
        }
    }

    if (socket_path == NULL || mip_address == 0) {
        print_mipd_usage();
        return 1;
    }

    init_arp_cache();

    printf("[mipd] socket path: %s\n", socket_path);
    printf("[mipd] local MIP address: %u\n", mip_address);
    printf("[mipd] debug mode: %s\n", debug_mode ? "enabled" : "disabled");

    int upper_socket = create_unix_socket(socket_path);
    if (upper_socket < 0) {
        return 1;
    }

    printf("[mipd] listening on upper-layer socket\n");

    while (1) {
        int client_fd = accept(upper_socket, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            break;
        }

        char buffer[256];
        ssize_t received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            if (received < 0) {
                perror("recv");
            }
            close(client_fd);
            continue;
        }

        buffer[received] = '\0';

        if (received < 1) {
            close(client_fd);
            continue;
        }

        uint8_t source_mip_address = (uint8_t)buffer[0];
        char *payload = buffer + 1;

        printf("[mipd] received upper-layer message: source=%u payload=%s\n",
               source_mip_address,
               payload);

        handle_upper_layer_message(socket_path, source_mip_address);

        if (debug_mode) {
            print_debug_state();
        }

        close(client_fd);
    }

    close(upper_socket);
    return 0;
}
