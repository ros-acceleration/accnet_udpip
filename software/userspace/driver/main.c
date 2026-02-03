// SPDX-License-Identifier: GPL-2.0+

/* main.c
 *
 * Example usage of userspace driver
 *
 * Copyright (C) Acceleration Robotics S.L.U
 * Copyright (C) Accelerat S.r.l.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>

#include "udriver.h"

// -----------------------------------------------------------------------------
// EXAMPLE SETUP
// -----------------------------------------------------------------------------

#define LOCAL_MAC               {0x02, 0x00, 0x00, 0x00, 0x00, 0x00}
#define LOCAL_IP                {192, 168, 1, 128}
#define LOCAL_SUBNET            {255, 255, 255, 0}
#define GW_IP                   {192, 168, 1, 2}

#define DEST_IP                 {192, 168, 1, 2}
#define LOCAL_PORT              1234
#define DEST_PORT               5678

#define LOCAL_PORT_MIN          1000
#define LOCAL_PORT_MAX          2000

#define PAYLOAD                 "Hello from KR260 PS, packet number 0"
#define PAYLOAD_SZ              sizeof(PAYLOAD)
#define PAYLOAD_SZ_QUAD_PADDED  ((PAYLOAD_SZ + 7) & ~7)

// -----------------------------------------------------------------------------
// EXAMPLE USAGE OF DRIVER
// -----------------------------------------------------------------------------

struct udp_packet tx_udp_packet;
struct udp_packet rx_udp_packet;

int main() 
{
    int packet_available;
    const uint8_t local_mac[ETH_ALEN] = LOCAL_MAC;
    const uint8_t local_ip[INET_ALEN] = LOCAL_IP;
    const uint8_t subnet_mask[INET_ALEN] = LOCAL_SUBNET;
    const uint8_t gw_ip[INET_ALEN] = GW_IP;
    const uint8_t dest_ip[INET_ALEN] = DEST_IP;

    // ---------------------------------------------------------
    // Initial device configuration
    // ---------------------------------------------------------
    
    if (udriver_initialize(
        local_mac, 
        local_ip, 
        subnet_mask, 
        gw_ip, 
        LOCAL_PORT_MIN, 
        LOCAL_PORT_MAX) == -1
    ) 
        exit(EXIT_FAILURE);
    
    // open socket at port 1234
    udriver_set_socket_status(LOCAL_PORT, UDRIVER_SOCKET_OPEN);
    
    // ---------------------------------------------------------
    // Send a packet
    // ---------------------------------------------------------

    // Build packet
    printf("> Building packet \n");

    memset(&tx_udp_packet, 0, sizeof(struct udp_packet));
    
    tx_udp_packet.payload_size_bytes = PAYLOAD_SZ_QUAD_PADDED;
    tx_udp_packet.source_ip          = htonl(*(uint32_t*)local_ip);
    tx_udp_packet.source_port        = LOCAL_PORT;
    tx_udp_packet.dest_ip            = htonl(*(uint32_t*)dest_ip);
    tx_udp_packet.dest_port          = DEST_PORT;
    
    memcpy(tx_udp_packet.payload, PAYLOAD, PAYLOAD_SZ);
    udriver_print_packet(&tx_udp_packet);

    // Send packet 0
    printf("> Sending packet \n");
    udriver_send(&tx_udp_packet);
    
    // Print IP register status
    printf("> Dump of register status \n");
    udriver_print_regs(LOCAL_PORT);
    
    // ---------------------------------------------------------
    // Receive a packet
    // ---------------------------------------------------------

    printf("> Receiving packet (polling) \n");

    // Wait and receive packet
    do
    {
        packet_available = udriver_recv(&rx_udp_packet, tx_udp_packet.source_port);
    } 
    while (packet_available == 0);
    
    udriver_print_packet(&rx_udp_packet);

    // Print IP register status
    printf("> Dump registers status \n");
    udriver_print_regs(LOCAL_PORT);

    // ---------------------------------------------------------
    // Cleanup and exit
    // ---------------------------------------------------------

    udriver_destroy();
    exit(EXIT_SUCCESS);
}
