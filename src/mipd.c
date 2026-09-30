#define _DEFAULT_SOURCE

#include "mip.h"
#include "mip_common.h"

#include <arpa/inet.h>
#include <errno.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

// Globals
static mip_arp_entry_t arp_cache[MIP_MAX_ARP_CACHE];
static int lower_socket_fd = -1;
static unsigned int local_mip_address = 0;
static int mip_ifindex = 0;
static uint8_t local_mac_address[6];

static int upper_client_fd = -1;

static const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Forward declarations
static void learn_arp_mapping(uint8_t mip_address, const uint8_t *mac_address, int ifindex);
static int lookup_arp_mapping(uint8_t mip_address, uint8_t mac_out[6], int *ifindex_out);
static void handle_incoming_mip_packet(const uint8_t *frame, size_t frame_length, int ifindex);

/**
 * Return the name of the first available network interface/adapter
 * out: pointer to buffer where the interface name will be stored.
 * out_len: capacity of the output buffer.
 *
 * Scans candidate interfaces (eth0, h1-eth0, lo, etc.) and picks the first active one[cite: 16].
 *
 * Returns 0 on success, or -1 if no suitable network interface is found[cite: 16].
 */
static int find_mip_interface_name(char *out, size_t out_len)
{
    static const char *const candidates[] = {
        "eth0",
        "A-eth0",
        "B-eth0",
        "C-eth0",
        "h1-eth0",
        "h2-eth0",
        "h3-eth0",
        "lo"};

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
    {
        if (if_nametoindex(candidates[i]) > 0)
        {
            snprintf(out, out_len, "%s", candidates[i]);
            return 0;
        }
    }

    return -1;
}

/**
 * Look up the hardware (MAC) address of a named network interface
 * ifname: pointer to string containing the interface name (e.g., "eth0")[cite: 16].
 * mac_out: pointer to 6-byte output buffer where MAC address will be written[cite: 16].
 *
 * Uses SIOCGIFHWADDR ioctl call over a temporary socket to fetch MAC address[cite: 16].
 *
 * Returns 0 on success, or -1 on socket/ioctl failure[cite: 16].
 */
static int get_interface_mac(const char *ifname, uint8_t mac_out[6])
{
    struct ifreq ifr;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        perror("socket (ioctl helper)");
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0)
    {
        perror("ioctl SIOCGIFHWADDR");
        close(fd);
        return -1;
    }

    memcpy(mac_out, ifr.ifr_hwaddr.sa_data, 6);
    close(fd);
    return 0;
}

/**
 * Initialize the global ARP cache
 *
 * Clears all entries in the global arp_cache array and marks them invalid[cite: 16].
 *
 * Affects global variables:
 * - arp_cache: writes zero/invalid state across all slots[cite: 16].
 *
 * Returns void.
 */
static void init_arp_cache(void)
{
    for (unsigned int i = 0; i < MIP_MAX_ARP_CACHE; ++i)
    {
        arp_cache[i].mip_address = 0;
        memset(arp_cache[i].mac_address, 0, sizeof(arp_cache[i].mac_address));
        arp_cache[i].valid = 0;
    }
}

/**
 * Create and transmit a complete MIP frame over the raw socket
 * ifindex: interface index to transmit the frame over[cite: 16].
 * destination_mip_address: target host MIP address[cite: 16].
 * source_mip_address: local host MIP address[cite: 16].
 * destination_mac: 6-byte destination hardware address[cite: 16].
 * sdu_type: service data unit type (e.g., ARP=1, Ping=2)[cite: 16].
 * payload: pointer to data buffer to transmit[cite: 16].
 * payload_length: byte length of payload data[cite: 16].
 *
 * Constructs Ethernet and MIP headers, pads SDU to 32-bit word alignment,
 * sets TTL (1 for broadcast, 64 for unicast), and transmits via sendto()[cite: 16].
 *
 * Affects global variables:
 * - lower_socket_fd: read to transmit frame via raw socket[cite: 16].
 * - local_mac_address: updated with refreshed local MAC address[cite: 16].
 *
 * Returns number of bytes sent on success, or -1 on error[cite: 16].
 */
static int send_mip_frame(int ifindex,
                          uint8_t destination_mip_address,
                          uint8_t source_mip_address,
                          const uint8_t destination_mac[6],
                          uint8_t sdu_type,
                          const void *payload,
                          size_t payload_length)
{
    uint8_t frame[2048];
    struct ether_header *eth_header = (struct ether_header *)frame;
    mip_header_t *header = (mip_header_t *)(frame + sizeof(struct ether_header));
    struct sockaddr_ll addr;

    if (lower_socket_fd < 0 || ifindex <= 0)
        return -1;

    struct ifreq ifr;
    if_indextoname(ifindex, ifr.ifr_name);
    get_interface_mac(ifr.ifr_name, local_mac_address);

    memset(frame, 0, sizeof(frame));
    memcpy(eth_header->ether_dhost, destination_mac, 6);
    memcpy(eth_header->ether_shost, local_mac_address, 6);
    eth_header->ether_type = htons(ETH_P_MIP);

    /* 1. Calculate SDU length in 32-bit words (rounded up) */
    uint16_t sdu_words = (payload_length + 3) / 4;
    size_t padded_payload_len = sdu_words * 4;

    /* 2. Set TTL: 1 for broadcast, 64 for unicast */
    uint8_t ttl = (destination_mip_address == MIP_ADDR_BROADCAST) ? 1 : 64;

    /* 3. Pack bitfields into MIP header struct */
    header->destination = destination_mip_address;
    header->source = source_mip_address;
    header->ttl_and_len_hi = (ttl << 4) | ((sdu_words >> 5) & 0x0F);
    header->len_lo_and_type = ((sdu_words & 0x1F) << 3) | (sdu_type & 0x07);

    if (payload_length > 0 && payload != NULL)
    {
        memcpy(frame + sizeof(struct ether_header) + sizeof(mip_header_t), payload, payload_length);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = ifindex;
    addr.sll_protocol = htons(ETH_P_MIP);
    addr.sll_halen = 6;
    memcpy(addr.sll_addr, destination_mac, 6);

    return sendto(lower_socket_fd, frame,
                  sizeof(struct ether_header) + sizeof(mip_header_t) + padded_payload_len,
                  0, (struct sockaddr *)&addr, sizeof(addr));
}

/**
 * Resolve target MIP address to MAC address via ARP cache or network broadcast
 * destination_mip_address: target host MIP address to resolve[cite: 16].
 * mac_out: 6-byte buffer where resolved MAC address is stored[cite: 16].
 * timeout_ms: timeout duration in milliseconds to wait for ARP reply[cite: 16].
 *
 * Checks local cache first; if missing, broadcasts MIP-ARP request and enters
 * a select() event loop to await matching ARP response before timeout[cite: 16].
 *
 * Affects global variables:
 * - arp_cache: read during lookup, updated via incoming ARP response[cite: 16].
 * - lower_socket_fd: read/write to broadcast ARP and receive responses[cite: 16].
 * - mip_ifindex, local_mip_address, broadcast_mac: read to send ARP request[cite: 16].
 *
 * Returns 0 on successful resolution, or -1 on error/timeout[cite: 16].
 */
static int resolve_mip_address(uint8_t destination_mip_address, uint8_t mac_out[6], int timeout_ms)
{
    mip_arp_message_t request;
    struct timeval start, now;

    int target_ifindex = 0;
    if (lookup_arp_mapping(destination_mip_address, mac_out, &target_ifindex) == 0)
    {
        return 0;
    }

    memset(&request, 0, sizeof(request));
    request.type = MIP_ARP_REQUEST;
    request.mip_address = destination_mip_address;

    printf("[mipd] no ARP entry for MIP %u; broadcasting MIP-ARP request\n", destination_mip_address);
    if (send_mip_frame(mip_ifindex, (uint8_t)MIP_ADDR_BROADCAST, (uint8_t)local_mip_address,
                       broadcast_mac, MIP_SDU_TYPE_ARP, &request, sizeof(request)) < 0)
    {
        return -1;
    }

    gettimeofday(&start, NULL);
    for (;;)
    {
        fd_set read_fds;
        struct timeval timeout;
        long elapsed_ms;

        gettimeofday(&now, NULL);
        elapsed_ms = (now.tv_sec - start.tv_sec) * 1000L + (now.tv_usec - start.tv_usec) / 1000L;
        if (elapsed_ms >= timeout_ms)
        {
            printf("[mipd] MIP-ARP request for %u timed out\n", destination_mip_address);
            return -1;
        }

        FD_ZERO(&read_fds);
        FD_SET(lower_socket_fd, &read_fds);
        timeout.tv_sec = (timeout_ms - elapsed_ms) / 1000;
        timeout.tv_usec = ((timeout_ms - elapsed_ms) % 1000) * 1000;

        int ready = select(lower_socket_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (ready < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("select (arp wait)");
            return -1;
        }
        if (ready == 0)
        {
            continue;
        }

        uint8_t raw_frame[2048];
        struct sockaddr_ll src_addr;
        socklen_t addr_len = sizeof(src_addr);

        ssize_t received = recvfrom(lower_socket_fd, raw_frame, sizeof(raw_frame), 0,
                                    (struct sockaddr *)&src_addr, &addr_len);

        if (received > 0)
        {
            if (src_addr.sll_pkttype != PACKET_OUTGOING)
            {
                handle_incoming_mip_packet(raw_frame, (size_t)received, src_addr.sll_ifindex);
            }
        }

        if (lookup_arp_mapping(destination_mip_address, mac_out, &target_ifindex) == 0)
        {
            return 0;
        }
    }
}

/**
 * Handle data received from the upper-layer application
 * source_mip_address: source host MIP address[cite: 16].
 * destination_mip_address: destination host MIP address[cite: 16].
 * payload: pointer to application payload string/data[cite: 16].
 * payload_length: byte size of application payload[cite: 16].
 *
 * Resolves destination MAC address via MIP-ARP and transmits payload as a Ping frame[cite: 16].
 * Drops packet if address resolution fails[cite: 16].
 *
 * Affects global variables:
 * - Indirectly uses arp_cache, lower_socket_fd, mip_ifindex, local_mip_address[cite: 16].
 *
 * Returns void.
 */
static void handle_upper_layer_message(uint8_t source_mip_address,
                                       uint8_t destination_mip_address,
                                       const char *payload,
                                       size_t payload_length)
{
    uint8_t destination_mac[6];
    int send_ifindex = 0;

    printf("[mipd] handle_upper_layer_message(): source=%u dest=%u payload=%.*s\n",
           source_mip_address, destination_mip_address, (int)payload_length,
           payload != NULL ? payload : "(null)");

    if (resolve_mip_address(destination_mip_address, destination_mac, 1000) < 0)
    {
        printf("[mipd] could not resolve MIP %u; dropping message\n", destination_mip_address);
        return;
    }

    lookup_arp_mapping(destination_mip_address, destination_mac, &send_ifindex);

    if (send_mip_frame(send_ifindex, destination_mip_address, source_mip_address, destination_mac,
                       MIP_SDU_TYPE_PING, payload, payload_length) == 0)
    {
        printf("  sent packet to peer\n");
    }
}

/**
 * Reply to an incoming MIP-ARP request querying local host's address
 * requester_mip: MIP address of requesting host[cite: 16].
 * requester_mac: 6-byte MAC address of requesting host[cite: 16].
 * incoming_ifindex: interface index where request was received[cite: 16].
 *
 * Constructs and transmits a unicast MIP-ARP Response containing local MIP address[cite: 16].
 *
 * Affects global variables:
 * - local_mip_address: read as response payload[cite: 16].
 * - lower_socket_fd: read via send_mip_frame[cite: 16].
 *
 * Returns void.
 */
static void handle_arp_request(uint8_t requester_mip, const uint8_t requester_mac[6], int incoming_ifindex)
{
    mip_arp_message_t response;
    memset(&response, 0, sizeof(response));

    response.type = MIP_ARP_RESPONSE;
    response.mip_address = (uint8_t)local_mip_address;

    printf("[mipd] replying to MIP-ARP request from %u\n", requester_mip);
    send_mip_frame(incoming_ifindex, requester_mip, (uint8_t)local_mip_address, requester_mac,
                   MIP_SDU_TYPE_ARP, &response, sizeof(response));
}

/**
 * Parse and route incoming MIP packet frame
 * frame: pointer to raw received network packet frame[cite: 16].
 * frame_length: total byte size of received frame[cite: 16].
 * ifindex: interface index where packet arrived[cite: 16].
 *
 * Auto-learns source ARP mapping, processes ARP requests/responses, and passes
 * Ping SDU messages to upper-layer application socket[cite: 16].
 *
 * Affects global variables:
 * - arp_cache: modified via learn_arp_mapping[cite: 16].
 * - local_mip_address: read to check packet destination[cite: 16].
 * - upper_client_fd: read/used to forward message to client application[cite: 16].
 *
 * Returns void.
 */
static void handle_incoming_mip_packet(const uint8_t *frame, size_t frame_length, int ifindex)
{
    if (frame == NULL || frame_length < sizeof(struct ether_header) + sizeof(mip_header_t))
        return;

    const struct ether_header *eth_header = (const struct ether_header *)frame;
    const mip_header_t *header = (const mip_header_t *)(frame + sizeof(struct ether_header));

    /* Unpack bitfields */
    uint16_t sdu_words = ((header->ttl_and_len_hi & 0x0F) << 5) | ((header->len_lo_and_type >> 3) & 0x1F);
    uint8_t sdu_type = header->len_lo_and_type & 0x07;
    size_t payload_length = sdu_words * 4;

    const uint8_t *payload = frame + sizeof(struct ether_header) + sizeof(mip_header_t);

    if (header->source != 0 && header->source != MIP_ADDR_BROADCAST)
    {
        learn_arp_mapping(header->source, eth_header->ether_shost, ifindex);
    }

    if (sdu_type == MIP_SDU_TYPE_ARP)
    {
        if (payload_length < sizeof(mip_arp_message_t))
            return;

        mip_arp_message_t msg;
        memcpy(&msg, payload, sizeof(msg));

        if (msg.type == MIP_ARP_REQUEST && msg.mip_address == local_mip_address)
        {
            handle_arp_request(header->source, eth_header->ether_shost, ifindex);
        }
        return;
    }

    if (sdu_type == MIP_SDU_TYPE_PING &&
        (header->destination == local_mip_address || header->destination == MIP_ADDR_BROADCAST))
    {
        if (upper_client_fd < 0)
            return;

        char up_message[257];
        up_message[0] = (char)header->source;
        size_t copy_len = payload_length < sizeof(up_message) - 1 ? payload_length : sizeof(up_message) - 1;
        memcpy(up_message + 1, payload, copy_len);

        send(upper_client_fd, up_message, copy_len + 1, 0);
    }
}

/**
 * Cache or update a MIP-to-MAC address mapping entry
 * mip_address: MIP address to store[cite: 16].
 * mac_address: pointer to 6-byte MAC address[cite: 16].
 * ifindex: interface index associated with mapping[cite: 16].
 *
 * Updates existing matching entry if present, or writes to first empty entry[cite: 16].
 * Silently ignores request if cache is full or mac_address is NULL[cite: 16].
 *
 * Affects global variables:
 * - arp_cache: written/updated with new entry[cite: 16].
 *
 * Returns void.
 */
static void learn_arp_mapping(uint8_t mip_address, const uint8_t *mac_address, int ifindex)
{
    if (mac_address == NULL)
        return;

    for (unsigned int i = 0; i < MIP_MAX_ARP_CACHE; ++i)
    {
        if (arp_cache[i].valid && arp_cache[i].mip_address == mip_address)
        {
            memcpy(arp_cache[i].mac_address, mac_address, 6);
            arp_cache[i].ifindex = ifindex;
            return;
        }
    }

    for (unsigned int i = 0; i < MIP_MAX_ARP_CACHE; ++i)
    {
        if (!arp_cache[i].valid)
        {
            arp_cache[i].mip_address = mip_address;
            memcpy(arp_cache[i].mac_address, mac_address, 6);
            arp_cache[i].ifindex = ifindex;
            arp_cache[i].valid = 1;
            return;
        }
    }
}

/**
 * Look up MAC address for a given MIP address in local cache
 * mip_address: MIP address to search for[cite: 16].
 * mac_out: pointer to 6-byte buffer where matched MAC address is copied[cite: 16].
 * ifindex_out: optional output pointer to store matching interface index[cite: 16].
 *
 * Searches valid entries in local arp_cache table[cite: 16].
 *
 * Affects global variables:
 * - arp_cache: read to check valid cached mappings[cite: 16].
 *
 * Returns 0 on success (hit), or -1 if entry is not found (miss)[cite: 16].
 */
static int lookup_arp_mapping(uint8_t mip_address, uint8_t mac_out[6], int *ifindex_out)
{
    for (unsigned int i = 0; i < MIP_MAX_ARP_CACHE; ++i)
    {
        if (arp_cache[i].valid && arp_cache[i].mip_address == mip_address)
        {
            memcpy(mac_out, arp_cache[i].mac_address, 6);
            if (ifindex_out)
                *ifindex_out = arp_cache[i].ifindex;
            return 0;
        }
    }
    return -1;
}

/**
 * Print debug information showing current state of ARP cache table
 *
 * Outputs index, MIP address, MAC address, and interface index for valid entries[cite: 16].
 *
 * Affects global variables:
 * - arp_cache: read to print current cache contents[cite: 16].
 *
 * Returns void.
 */
static void print_debug_state(void)
{
    printf("[mipd] ARP cache:\n");

    for (unsigned int i = 0; i < MIP_MAX_ARP_CACHE; ++i)
    {
        if (arp_cache[i].valid)
        {
            printf("  [%u] mip=%u mac=%02X:%02X:%02X:%02X:%02X:%02X ifindex=%d\n",
                   i,
                   arp_cache[i].mip_address,
                   arp_cache[i].mac_address[0],
                   arp_cache[i].mac_address[1],
                   arp_cache[i].mac_address[2],
                   arp_cache[i].mac_address[3],
                   arp_cache[i].mac_address[4],
                   arp_cache[i].mac_address[5],
                   arp_cache[i].ifindex);
        }
        else
        {
            printf("  [%u] empty\n", i);
        }
    }
}

/**
 * Create, bind, and set up a Unix domain IPC socket
 * socket_path: filesystem path for Unix socket file[cite: 16].
 *
 * Unlinks old path if present, creates AF_UNIX streaming socket, binds, and listens[cite: 16].
 *
 * Returns valid file descriptor on success, or -1 on error[cite: 16].
 */
static int create_unix_socket(const char *socket_path)
{
    int fd;
    struct sockaddr_un addr;
    if (socket_path == NULL)
    {
        return -1;
    }

    unlink(socket_path);

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 5) < 0)
    {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

/**
 * Create and bind raw MIP packet socket
 *
 * Creates AF_PACKET raw socket bound to ETH_P_MIP protocol across all interfaces[cite: 16].
 *
 * Returns valid raw socket file descriptor on success, or -1 on error[cite: 16].
 */
static int create_raw_mip_socket(void)
{
    int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_MIP));
    if (fd < 0)
    {
        perror("socket raw");
        return -1;
    }

    struct sockaddr_ll addr;
    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_MIP);
    addr.sll_ifindex = 0;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind raw");
        close(fd);
        return -1;
    }

    return fd;
}

/**
 * Program main entry point and non-blocking I/O event loop
 * argc: command line argument count[cite: 16].
 * argv: command line argument string array[cite: 16].
 *
 * Parses arguments (-d, socket path, MIP address), initializes state/sockets,
 * and runs main select() loop to process incoming network and application I/O[cite: 16].
 *
 * Affects global variables:
 * - local_mip_address, mip_ifindex, local_mac_address, lower_socket_fd, upper_client_fd[cite: 16].
 * - arp_cache: initialized at startup[cite: 16].
 *
 * Returns 0 on clean exit, or 1 on argument/initialization failure[cite: 16].
 */
int main(int argc, char **argv)
{
    const char *socket_path = NULL;
    unsigned int mip_address = 0;
    int debug_mode = 0;
    char ifname[IF_NAMESIZE];

    if (argc < 2)
    {
        print_mipd_usage();
        return 1;
    }

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-h") == 0)
        {
            print_mipd_usage();
            return 0;
        }

        if (strcmp(argv[i], "-d") == 0)
        {
            debug_mode = 1;
            continue;
        }

        if (socket_path == NULL)
        {
            socket_path = argv[i];
            continue;
        }

        if (mip_address == 0 && argv[i][0] != '\0')
        {
            mip_address = (unsigned int)strtoul(argv[i], NULL, 10);
            continue;
        }
    }

    if (socket_path == NULL || mip_address == 0)
    {
        print_mipd_usage();
        return 1;
    }

    init_arp_cache();
    local_mip_address = mip_address;
    lower_socket_fd = -1;

    if (find_mip_interface_name(ifname, sizeof(ifname)) < 0)
    {
        fprintf(stderr, "[mipd] no usable network interface found\n");
        return 1;
    }
    mip_ifindex = (int)if_nametoindex(ifname);
    if (get_interface_mac(ifname, local_mac_address) < 0)
    {
        fprintf(stderr, "[mipd] could not read MAC address of %s\n", ifname);
        return 1;
    }

    printf("[mipd] socket path: %s\n", socket_path);
    printf("[mipd] local MIP address: %u\n", mip_address);
    printf("[mipd] interface: %s (MAC %02X:%02X:%02X:%02X:%02X:%02X)\n", ifname,
           local_mac_address[0], local_mac_address[1], local_mac_address[2],
           local_mac_address[3], local_mac_address[4], local_mac_address[5]);
    printf("[mipd] debug mode: %s\n", debug_mode ? "enabled" : "disabled");

    int upper_socket = create_unix_socket(socket_path);
    int lower_socket = create_raw_mip_socket();
    if (upper_socket < 0)
    {
        return 1;
    }
    if (lower_socket < 0)
    {
        close(upper_socket);
        return 1;
    }

    lower_socket_fd = lower_socket;

    printf("[mipd] listening on upper-layer socket\n");
    printf("[mipd] listening on raw MIP socket\n");

    while (1)
    {
        fd_set read_fds;
        int max_fd;
        struct timeval timeout;

        FD_ZERO(&read_fds);
        FD_SET(upper_socket, &read_fds);
        FD_SET(lower_socket, &read_fds);
        max_fd = upper_socket > lower_socket ? upper_socket : lower_socket;
        if (upper_client_fd >= 0)
        {
            FD_SET(upper_client_fd, &read_fds);
            if (upper_client_fd > max_fd)
            {
                max_fd = upper_client_fd;
            }
        }

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        if (select(max_fd + 1, &read_fds, NULL, NULL, &timeout) < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("select");
            break;
        }

        if (FD_ISSET(lower_socket, &read_fds))
        {
            uint8_t raw_frame[2048];
            struct sockaddr_ll src_addr;
            socklen_t addr_len = sizeof(src_addr);
            ssize_t received = recvfrom(lower_socket, raw_frame, sizeof(raw_frame), 0,
                                        (struct sockaddr *)&src_addr, &addr_len);
            if (received > 0)
            {
                if (src_addr.sll_pkttype != PACKET_OUTGOING)
                {
                    handle_incoming_mip_packet(raw_frame, (size_t)received, src_addr.sll_ifindex);
                }
            }
        }

        if (upper_client_fd >= 0 && FD_ISSET(upper_client_fd, &read_fds))
        {
            char buffer[256];
            ssize_t received = recv(upper_client_fd, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0)
            {
                if (received < 0)
                {
                    perror("recv");
                }
                printf("[mipd] upper-layer client disconnected\n");
                close(upper_client_fd);
                upper_client_fd = -1;
            }
            else
            {
                buffer[received] = '\0';
                uint8_t destination_mip_address = (uint8_t)buffer[0];
                uint8_t source_mip_address = (uint8_t)mip_address;
                char *payload = buffer + 1;
                size_t payload_length = (size_t)received - 1;

                printf("[mipd] received upper-layer message: source=%u dest=%u payload=%.*s\n",
                       source_mip_address, destination_mip_address, (int)payload_length, payload);

                handle_upper_layer_message(source_mip_address, destination_mip_address, payload, payload_length);

                if (debug_mode)
                {
                    print_debug_state();
                }
            }
        }

        if (FD_ISSET(upper_socket, &read_fds))
        {
            int client_fd = accept(upper_socket, NULL, NULL);
            if (client_fd < 0)
            {
                perror("accept");
                break;
            }

            if (upper_client_fd >= 0)
            {
                printf("[mipd] rejecting extra upper-layer connection\n");
                close(client_fd);
            }
            else
            {
                printf("[mipd] upper-layer client connected\n");
                upper_client_fd = client_fd;
            }
        }
    }

    if (upper_client_fd >= 0)
    {
        close(upper_client_fd);
    }
    close(lower_socket);
    close(upper_socket);
    return 0;
}