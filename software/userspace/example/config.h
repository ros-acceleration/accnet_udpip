/* 
 * Configuration macros for example scripts.
 *
 * Copyright (C) Accelerat S.r.l.
 */

#define INTERFACE_NAME          "udpip0"                                // Name of the interface to be used with raw sockets

#define LOCAL_MAC               {0x02, 0x00, 0x00, 0x00, 0x00, 0x00}    // MAC address of the local interface
#define DEST_MAC                {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}    // MAC address of the destination interface

#define LOCAL_IP                "192.168.1.128"                         // Local interface IP address
#define LOCAL_PORT              7400                                    // Local port number
#define DEST_IP                 "192.168.1.2"                           // Destination interface IP address
#define DEST_PORT               7410                                    // Destination port number

#define MAX_PAYLOAD_SIZE        1440                                    // UDP packet payload max size
#define PAYLOAD                 "Hello from KR260 PS"                   // Payload sent over network
#define PAYLOAD_SIZE_LEN(str)   (strlen(str))                           // Actual payload len

/**
 * udp_core_raw_packet represents the complete structure of a UDP packet 
 * (L2 + L3 + L4) and its payload.
 */

struct __attribute__((packed)) udp_core_raw_packet
{
    unsigned char dest_mac[ETH_ALEN];
    unsigned char src_mac[ETH_ALEN];
    unsigned short ether_type;
    unsigned char ihl : 4;
    unsigned char version : 4;
    unsigned char tos;
    unsigned short total_len;
    unsigned short ident;
    unsigned char flags : 4;
    unsigned short frag_offset : 12;
    unsigned char ttl;
    unsigned char protocol;
    unsigned short checksum;
    unsigned int source_ip;
    unsigned int dest_ip;
    unsigned short source_port;
    unsigned short dest_port;
    unsigned short payload_len;
    unsigned short udp_checksum;
    char payload[MAX_PAYLOAD_SIZE];
};