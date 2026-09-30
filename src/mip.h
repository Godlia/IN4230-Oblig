#ifndef MIP_H
#define MIP_H

#include <stdint.h>

#define ETH_P_MIP 0x88B5
#define MIP_ADDR_BROADCAST 255u
#define MIP_MAX_HOSTS 255u
#define MIP_MAX_ARP_CACHE 16u

#define MIP_SDU_TYPE_ARP  0x01u
#define MIP_SDU_TYPE_PING 0x02u

#define MIP_ARP_REQUEST  0x00u
#define MIP_ARP_RESPONSE 0x01u

/* MIP-ARP: 1 bit type + 8 bit address + 23 bit zeroes = 32 bits (4 bytes) */
typedef struct {
    uint8_t type;         /* 0x00 (Request) eller 0x01 (Response) */
    uint8_t mip_address;
    uint8_t reserved[3];  /* 23 bits utfylling med nuller */
} __attribute__((packed)) mip_arp_message_t;

/* MIP Header: Dest (8b), Src (8b), TTL (4b), SDU Len (9b), SDU Type (3b) = 32 bits (4 bytes) */
typedef struct {
    uint8_t destination;
    uint8_t source;
    uint8_t ttl_and_len_hi; /* [TTL: 4 bits] [SDU Len high: 4 bits] */
    uint8_t len_lo_and_type; /* [SDU Len low: 5 bits] [SDU Type: 3 bits] */
} __attribute__((packed)) mip_header_t;

typedef struct {
    uint8_t mip_address;
    uint8_t mac_address[6];
    int ifindex;
    int valid;
} mip_arp_entry_t;

#endif