#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <math.h>

#define CYCLE_DURATION 10 // seconds
#define CYCLE_NUMBER 10 // number
#define INTERFACE_NAME "udpip0"
#define LATENCY_ITERATIONS 1000
#define MAX_LATENCY_PACKET_SIZE 2048

typedef struct 
{
    const char* ip;
    short port;
    int packet_size;
    int duration;
    int use_raw;
} client_args_t;

#define MAX_PAYLOAD_SIZE        1440 

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

struct udp_core_raw_packet packet;

void* client_thread_func(void* arg)
{
    client_args_t* args = (client_args_t*) arg;
    int sockfd;
    int count;
    struct sockaddr_in dst_addr;
    char* buffer = malloc(args->packet_size);
    time_t start_time, now;
    ssize_t sent;

    if (args->use_raw) 
    {
        sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    } 
    else 
    {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    }

    if (sockfd < 0) 
    {
        perror("Socket creation failed");
        free(buffer);
        pthread_exit(NULL);
    }

    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_addr.s_addr = inet_addr(args->ip);
    dst_addr.sin_port = htons(args->port);

    memset(buffer, 'A', args->packet_size);
    time(&start_time);

    count = 0;

    while (1) 
    {
        sent = sendto(sockfd, buffer, args->packet_size, 0, (struct sockaddr*)&dst_addr, sizeof(dst_addr));
        
        if (sent < 0) 
        {
            perror("Send failed");
            continue;
        }

        time(&now);
        
        if (difftime(now, start_time) >= args->duration)
        {
            time(&start_time);
            count++;
        }

        if (count > CYCLE_NUMBER)
            break;
    }

    free(buffer);
    close(sockfd);
    pthread_exit(NULL);
}

void run_client(const char* ip, short port, int pkt_size, int threads, int use_raw)
{
    pthread_t* thread_ids = malloc(sizeof(pthread_t) * threads);
    client_args_t args = { ip, port, pkt_size, CYCLE_DURATION, use_raw };

    printf("Running bandwidth test to %s:%d with %d thread(s), packet size: %d bytes, raw: %s\n",
           ip, port, threads, pkt_size, use_raw ? "YES" : "NO");

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

void run_server(const char* ip, short port, int pkt_size, int use_raw)
{
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);
    char* buffer = malloc(pkt_size);
    ssize_t recvd;
    size_t total_bytes = 0;
    time_t start_time, now;
    int retval;

    struct ifreq ifr;
    struct sockaddr_ll socket_address;

    if (use_raw == 1)
    {
        sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    }
    else
    {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    }

    if (sockfd < 0) 
    {
        perror("Socket creation failed");
        free(buffer);
        exit(EXIT_FAILURE);
    }

    if (use_raw)
    {
        // clear and set up the ifreq structure to specify the interface
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, INTERFACE_NAME, IFNAMSIZ-1);
        
        // get the interface index (this is needed for raw sockets)
        if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) 
        {
            perror("ioctl SIOCGIFINDEX failed");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        // set up the sockaddr_ll structure
        memset(&socket_address, 0, sizeof(socket_address));
        socket_address.sll_family = AF_PACKET;
        socket_address.sll_protocol = htons(ETH_P_IP);
        socket_address.sll_ifindex = ifr.ifr_ifindex;

        retval = bind(sockfd, (struct sockaddr *)&socket_address, sizeof(socket_address));
    }
    else
    {
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(ip);
        server_addr.sin_port = htons(port);

        retval = bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    }

    if (retval < 0) 
    {
        perror("Bind failed");
        close(sockfd);
        free(buffer);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on %s:%d\n", ip, port);
    time(&start_time);

    while (1) 
    {
        if (use_raw)
        {
            recvd = recvfrom(sockfd, &packet, sizeof(struct udp_core_raw_packet), 0, NULL, NULL);
        }
        else
        {
            recvd = recvfrom(sockfd, buffer, pkt_size, 0, (struct sockaddr*)&client_addr, &addrlen);
        }
    
        if (recvd > 0)
            total_bytes += recvd;

        time(&now);
    
        if (difftime(now, start_time) >= CYCLE_DURATION) 
        {
            double mbps = (total_bytes * 8.0) / (1000000.0 * CYCLE_DURATION);
            printf("Received %.2f MB in %d seconds: %.2f Mbps\n", total_bytes / (1024.0 * 1024.0), CYCLE_DURATION, mbps);
            total_bytes = 0;
            time(&start_time);
        }
    }

    close(sockfd);
    free(buffer);
}

void run_latency_server(const char* ip, short port)
{
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);
    char buffer[MAX_LATENCY_PACKET_SIZE];
    ssize_t recvd;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Latency server listening on %s:%d\n", ip, port);

    while (1)
    {
        recvd = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &addrlen);
        if (recvd < 0)
        {
            perror("Receive failed");
            continue;
        }

        sendto(sockfd, buffer, recvd, 0, (struct sockaddr*)&client_addr, addrlen);
    }

    close(sockfd);
}

void run_latency_client(const char* ip, short port, int pkt_size)
{
    int sockfd;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    char send_buf[MAX_LATENCY_PACKET_SIZE];
    char recv_buf[MAX_LATENCY_PACKET_SIZE];
    socklen_t addr_len = sizeof(server_addr);
    struct sockaddr_in sock_addr;
    struct timespec start, end;
    double total_rtt = 0, min_rtt = 1e9, max_rtt = 0;
    double jitter_sum = 0;
    double last_rtt = -1;

    if (pkt_size > MAX_LATENCY_PACKET_SIZE)
    {
        fprintf(stderr, "Packet size too large for latency test (max %d bytes)\n", MAX_LATENCY_PACKET_SIZE);
        return;
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0)
    {
        perror("Socket creation failed");
        return;
    }

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(port+1);
    client_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0)
    {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    memset(send_buf, 'L', pkt_size);

    printf("Running latency test to %s:%d with %d iterations, packet size: %d bytes\n",
           ip, port, LATENCY_ITERATIONS, pkt_size);

    for (int i = 0; i < LATENCY_ITERATIONS; ++i)
    {
        clock_gettime(CLOCK_MONOTONIC, &start);

        ssize_t sent = sendto(sockfd, send_buf, pkt_size, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
        if (sent < 0)
        {
            perror("Send failed");
            continue;
        }

        ssize_t recvd = recvfrom(sockfd, recv_buf, pkt_size, 0, (struct sockaddr*)&sock_addr, &addr_len);
        if (recvd < 0)
        {
            perror("Receive failed");
            continue;
        }

        clock_gettime(CLOCK_MONOTONIC, &end);

        double rtt_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                        (end.tv_nsec - start.tv_nsec) / 1e6;

        if (rtt_ms < min_rtt) min_rtt = rtt_ms;
        if (rtt_ms > max_rtt) max_rtt = rtt_ms;
        total_rtt += rtt_ms;

        if (last_rtt >= 0)
        {
            jitter_sum += fabs(rtt_ms - last_rtt);
        }

        last_rtt = rtt_ms;
        usleep(100);
    }

    double avg_rtt = total_rtt / LATENCY_ITERATIONS;
    double jitter = jitter_sum / (LATENCY_ITERATIONS - 1);

    printf("Latency test complete.\n");
    printf("RTT:   min = %.3f ms, avg = %.3f ms, max = %.3f ms\n", min_rtt, avg_rtt, max_rtt);
    printf("Jitter (avg RTT variation): %.3f ms\n", jitter);

    close(sockfd);
}


int main(int argc, char** argv)
{
    if (argc < 6) 
    {
        printf("Usage:\n");
        printf("  %s bandwidth <server|client> <ip> <port> <packet_size> [raw] [threads]\n", argv[0]);
        printf("  %s latency <server|client> <ip> <port> <packet_size>\n", argv[0]);
        return 1;
    }

    const char* test_type = argv[1];
    const char* mode = argv[2];
    const char* ip = argv[3];
    short port = atoi(argv[4]);
    int pkt_size = atoi(argv[5]);

    if (pkt_size <= 0 || pkt_size > 65507) 
    {
        fprintf(stderr, "Invalid packet size: must be > 0 and <= 65507 \n");
        return 1;
    }

    if (strcmp(test_type, "bandwidth") == 0)
    {
        int use_raw = (argc >= 7 && strcmp(argv[6], "raw") == 0);

        if (strcmp(mode, "server") == 0) 
        {
            run_server(ip, port, pkt_size, use_raw);
        } 
        else if (strcmp(mode, "client") == 0)
        {
            int threads = (argc >= 8) ? atoi(argv[7]) : 1;
            
            if (threads < 1) 
                threads = 1;
            
            run_client(ip, port, pkt_size, threads, use_raw);
        } 
        else 
        {
            fprintf(stderr, "Invalid mode. Use 'server' or 'client'.\n");
            return 1;
        }
    }
    else if (strcmp(test_type, "latency") == 0)
    {
        if (strcmp(mode, "server") == 0)
        {
            run_latency_server(ip, port);
        }
        else if (strcmp(mode, "client") == 0)
        {
            run_latency_client(ip, port, pkt_size);
        }
        else
        {
            fprintf(stderr, "Invalid mode. Use 'server' or 'client'.\n");
            return 1;
        }
    }
    else
    {
        fprintf(stderr, "Invalid test type. Use 'bandwidth' or 'latency'.\n");
        return 1;
    }

    return 0;
}
