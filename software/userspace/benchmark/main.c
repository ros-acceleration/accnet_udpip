// SPDX-License-Identifier: GPL-2.0+

/* main.c
 *
 * Example usage of userspace driver
 *
 * Copyright (C) Acceleration Robotics S.L.U
 * Copyright (C) Accelerat S.r.l.
 */
#define _GNU_SOURCE

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

#include <xrt/xrt_device.h>
#include <xrt/xrt_bo.h>

#include "udriver.h"

#define CYCLE_DURATION 10 // seconds
#define CYCLE_NUMBER 5 // number

typedef struct 
{
    const char* ip;
    short port;
    int packet_size;
    int duration;
} client_args_t;

// -----------------------------------------------------------------------------
// EXAMPLE SETUP
// -----------------------------------------------------------------------------

#define LOCAL_MAC               {0x02, 0x00, 0x00, 0x00, 0x00, 0x00}
#define LOCAL_IP                {192, 168, 1, 128}
#define LOCAL_SUBNET            {255, 255, 255, 0}
#define GW_IP                   {192, 168, 1, 2}

#define DEST_IP                 {192, 168, 1, 2}
#define LOCAL_PORT              7410
#define DEST_PORT               5678

#define LOCAL_PORT_MIN          7400
#define LOCAL_PORT_MAX          7500

#define PAYLOAD                 "Hello from KR260 PS, packet number 0"
#define PAYLOAD_SZ              sizeof(PAYLOAD)
#define PAYLOAD_SZ_QUAD_PADDED  ((PAYLOAD_SZ + 7) & ~7)

struct udp_packet tx_udp_packet;
struct udp_packet rx_udp_packet;

void nsleep(uint64_t nanoseconds)
{
    struct timespec duration;
    duration.tv_nsec = nanoseconds;
    duration.tv_sec = 0;

    nanosleep(&duration, NULL);
}

void* client_thread_func(void* arg)
{
    client_args_t* args = (client_args_t*) arg;

    const uint8_t local_mac[ETH_ALEN] = LOCAL_MAC;
    const uint8_t local_ip[INET_ALEN] = LOCAL_IP;
    const uint8_t subnet_mask[INET_ALEN] = LOCAL_SUBNET;
    const uint8_t gw_ip[INET_ALEN] = GW_IP;
    const uint8_t dest_ip[INET_ALEN] = DEST_IP;

    time_t start_time, now;
    int count;
    ssize_t sent;
    uint64_t packets = 0;

    if (udriver_initialize(
        local_mac, 
        local_ip, 
        subnet_mask, 
        gw_ip, 
        LOCAL_PORT_MIN, 
        LOCAL_PORT_MAX) == -1
    ) 
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    } 
    
    udriver_set_socket_status(LOCAL_PORT, UDRIVER_SOCKET_OPEN);
    
    time(&start_time);

    count = 0;

    tx_udp_packet.payload_size_bytes = UDP_PAYL_MAX_LEN;
    tx_udp_packet.source_ip          = htonl(*(uint32_t*)local_ip);
    tx_udp_packet.source_port        = LOCAL_PORT;
    tx_udp_packet.dest_ip            = htonl(*(uint32_t*)dest_ip);
    tx_udp_packet.dest_port          = args->port;
        
    memset(tx_udp_packet.payload, 'A', UDP_PAYL_MAX_LEN);
    
    while (1) 
    {
        sent = udriver_send(&tx_udp_packet);
        
        if (sent < 0) 
        {
            perror("Send failed");
            continue;
        }

        time(&now);
        packets++;
        
        if (difftime(now, start_time) >= args->duration)
        {
            time(&start_time);
            count++;
        }

        if (count > CYCLE_NUMBER)
            break;
    }

    printf("Benchmark end - Packets sent %lu \n", packets);
        
    udriver_destroy();
    pthread_exit(NULL);
}

void run_client(const char* ip, short port, int pkt_size, int threads)
{
    pthread_t* thread_ids = malloc(sizeof(pthread_t) * threads);
    client_args_t args = { ip, port, pkt_size, CYCLE_DURATION };

    printf("Running bandwidth test to %s:%d with %d thread(s), packet size: %d bytes \n",
           ip, port, threads, pkt_size);

    for (int i = 0; i < threads; ++i) 
    {
        pthread_create(&thread_ids[i], NULL, client_thread_func, &args);
    }

    for (int i = 0; i < threads; ++i) 
    {
        pthread_join(thread_ids[i], NULL);
    }

    printf("Client transmission finished.\n");
    free(thread_ids);
}

void run_server(const char* ip, short port)
{
    const uint8_t local_mac[ETH_ALEN] = LOCAL_MAC;
    const uint8_t local_ip[INET_ALEN] = LOCAL_IP;
    const uint8_t subnet_mask[INET_ALEN] = LOCAL_SUBNET;
    const uint8_t gw_ip[INET_ALEN] = GW_IP;

    size_t total_bytes = 0;
    time_t start_time, now;
    ssize_t recvd;
    uint64_t packets = 0;
    uint64_t wasted = 0;
    uint64_t cycles = 0;

    if (udriver_initialize(
        local_mac, 
        local_ip, 
        subnet_mask, 
        gw_ip, 
        LOCAL_PORT_MIN, 
        LOCAL_PORT_MAX) == -1
    ) 
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    } 
    
    udriver_set_socket_status(LOCAL_PORT, UDRIVER_SOCKET_OPEN);

    printf("Server listening on %s:%d\n", ip, port);
    time(&start_time);

    while (1) 
    {    
        recvd = udriver_recv(&rx_udp_packet, LOCAL_PORT);
        cycles++;
    
        if (recvd > 0)
        {
            total_bytes += recvd;
            packets++;
        }
        else if (recvd == 0)
        {
            wasted++;
            nsleep(1);
        }

        time(&now);
    
        if (difftime(now, start_time) >= CYCLE_DURATION) 
        {
            double mbps = (total_bytes * 8.0) / (1000000.0 * CYCLE_DURATION);
            double wasted_cycles = (double) wasted / cycles;
            printf("Received %lu packets - %.2f wasted cycles: %.2f MB in %d seconds: %.2f Mbps\n", packets, wasted_cycles, total_bytes / (1024.0 * 1024.0), CYCLE_DURATION, mbps);
            total_bytes = 0;
            wasted = 0;
            cycles = 0;
            time(&start_time);
        }
    }

    udriver_destroy();
}

int main(int argc, char** argv) 
{
    if (argc < 5) 
    {
        printf("Usage: %s <server|client> <ip> <port> <packet_size> [raw] [threads]\n", argv[0]);
        return 1;
    }

    const char* mode = argv[1];
    const char* ip = argv[2];
    short port = atoi(argv[3]);
    int pkt_size = atoi(argv[4]);

    if (pkt_size <= 0 || pkt_size > 65507) 
    {
        fprintf(stderr, "Invalid packet size: must be > 0 and <= 65507 \n");
        return 1;
    }

    if (strcmp(mode, "server") == 0) 
    {
        run_server(ip, port);
    } 
    else if (strcmp(mode, "client") == 0) 
    {
        int threads = (argc >= 7) ? atoi(argv[6]) : 1;
        
        if (threads < 1) 
            threads = 1;
        
        run_client(ip, port, pkt_size, threads);
    } 
    else 
    {
        fprintf(stderr, "Invalid mode. Use 'server' or 'client'.\n");
        return 1;
    }
 
    return 0;
}
