#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "config.h"

struct udp_core_raw_packet packet;

int main() 
{
    int sockfd;
    ssize_t ret;
    struct ifreq ifr;
    struct sockaddr_ll socket_address;
    
    char source_ip[INET_ADDRSTRLEN];
    char dest_ip[INET_ADDRSTRLEN];
    
    // create a raw socket using AF_PACKET
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    
    if (sockfd < 0) 
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

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

    // bind the socket to the specified interface
    if (bind(sockfd, (struct sockaddr *)&socket_address, sizeof(socket_address)) < 0) 
    {
        perror("binding socket to interface failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    while(1)
    {
        ret = recvfrom(sockfd, &packet, sizeof(struct udp_core_raw_packet), 0, NULL, NULL);
        
        if (ret < 0) 
        {
            perror("packet receive failed");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        printf("received packet of size %ld bytes on interface %s\n", ret, INTERFACE_NAME);

        if (packet.source_ip == 0 && packet.dest_ip == 0xFFFFFFFF)
        {
            printf("received DHCP packet - Discarded\n");
            continue;
        }

        inet_ntop(AF_INET, &(packet.source_ip), source_ip, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(packet.dest_ip), dest_ip, INET_ADDRSTRLEN);

        printf(" > packet source and destination IPs: %s - %s\n", source_ip, dest_ip);
        printf(" > packet source and destination ports: %d - %d\n", ntohs(packet.source_port), ntohs(packet.dest_port));
        printf(" > packet payload length: %d\n", ntohs(packet.payload_len));
        printf(" > packet payload: %s\n", packet.payload);
    }

    // clean up
    close(sockfd);
    return 0;
}

