#include "mip.h"
#include "mip_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    /*
     * TODO:
     * 1. Read a message from the UNIX socket interface.
     * 2. Extract the destination MIP address from the message.
     * 3. Build a MIP PDU with header + SDU.
     * 4. Look up the destination MAC using MIP-ARP.
     * 5. Send the Ethernet frame to the lower layer.
     */
    (void)socket_path;
    (void)source_mip_address;
    printf("[mipd] handle_upper_layer_message(): to be implemented.\n");
}

static void handle_incoming_mip_packet(const uint8_t *frame, size_t frame_length)
{
    /*
     * TODO:
     * 1. Parse the Ethernet header and verify ETH_P_MIP.
     * 2. Extract the MIP header and payload.
     * 3. Check if the packet is for this host or broadcast.
     * 4. Decide if it is an ordinary data packet or an ARP packet.
     * 5. Forward the payload to the upper layer or reply accordingly.
     */
    (void)frame;
    (void)frame_length;
    printf("[mipd] handle_incoming_mip_packet(): to be implemented.\n");
}

static void learn_arp_mapping(uint8_t mip_address, const uint8_t *mac_address)
{
    /*
     * TODO:
     * Insert or update an entry in the ARP cache.
     */
    (void)mip_address;
    (void)mac_address;
    printf("[mipd] learn_arp_mapping(): to be implemented.\n");
}

static void print_debug_state(void)
{
    /*
     * TODO:
     * Print the current ARP table and the current packet metadata.
     */
    printf("[mipd] debug state: ARP cache and packet trace go here.\n");
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

    /*
     * The program should continue running here, listening on:
     *  - RAW Ethernet socket for incoming MIP traffic
     *  - UNIX domain socket for communication with upper-layer apps
     */
    printf("[mipd] skeleton program is ready. Implement the main event loop next.\n");

    return 0;
}
