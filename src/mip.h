#ifndef MIP_H
#define MIP_H

#include <stdint.h>

#define ETH_P_MIP 0x88B5
#define MIP_ADDR_BROADCAST 255u
#define MIP_MAX_HOSTS 255u
#define MIP_MAX_ARP_CACHE 16u

/*
 * SDU types carried in a MIP PDU. This lets the receiving mipd tell an
 * ARP request/response apart from an ordinary upper-layer message
 * (e.g. a ping), which the original skeleton did not distinguish
 * (sdu_type was hardcoded to 1 for everything).
 */
#define MIP_SDU_TYPE_ARP  0x01u
#define MIP_SDU_TYPE_PING 0x02u

/* MIP-ARP message sub-types, carried as the SDU payload when
 * sdu_type == MIP_SDU_TYPE_ARP. */
#define MIP_ARP_REQUEST  0x00u
#define MIP_ARP_RESPONSE 0x01u

/*
 * MIP-ARP request/response payload.
 *
 * type:        MIP_ARP_REQUEST ("who has this MIP address?") or
 *              MIP_ARP_RESPONSE ("this MIP address is mine").
 * mip_address: the MIP address being queried (request) or announced
 *              (response).
 */
typedef struct {
    uint8_t type;
    uint8_t mip_address;
} mip_arp_message_t;

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
    int ifindex;             /* Interface index associated with this peer */
    int valid;
} mip_arp_entry_t;

#endif /* MIP_H */
