#ifndef MIP_H
#define MIP_H

#include <stdint.h>

#define ETH_P_MIP 0x88B5
#define MIP_ADDR_BROADCAST 255u
#define MIP_MAX_HOSTS 255u
#define MIP_MAX_ARP_CACHE 16u

/*
 * Minimal placeholder for the MIP header fields.
 *
 * The real assignment specifies destination/source MIP addresses, TTL,
 * SDU length, and SDU type. This simplified representation is enough for
 * structuring the implementation and for a beginner-friendly skeleton.
 */
typedef struct {
    uint8_t destination;   /* MIP address of the target host. */
    uint8_t source;        /* MIP address of the sender host. */
    uint8_t ttl;           /* Hop limit; decremented by routers if used later. */
    uint16_t sdu_length;   /* Length of the payload in bytes. */
    uint8_t sdu_type;      /* The kind of SDU being carried. */
} mip_header_t;

/*
 * MIP-ARP cache entry.
 *
 * In a real implementation, this would also store the Ethernet MAC address
 * of the peer and a validity/TTL marker.
 */
typedef struct {
    uint8_t mip_address;
    uint8_t mac_address[6];
    int valid;
} mip_arp_entry_t;

#endif /* MIP_H */
