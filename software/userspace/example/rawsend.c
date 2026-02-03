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
    
    uint32_t local_ip;
    uint32_t dest_ip;

    // ethernet header: destination MAC, source MAC, ether-type
    char payload[] = PAYLOAD;
    unsigned char dest_mac[6] = DEST_MAC;
    unsigned char src_mac[6] = LOCAL_MAC;
    unsigned short ether_type = htons(0x0800); 
    
    // create a raw socket using AF_PACKET
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    
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
    socket_address.sll_ifindex = ifr.ifr_ifindex;
    socket_address.sll_halen = ETH_ALEN;

    // construct the Ethernet frame
    memcpy(packet.dest_mac, dest_mac, ETH_ALEN);
    memcpy(packet.src_mac, src_mac, ETH_ALEN);
    memcpy(&packet.ether_type, &ether_type, 2);

    // prepare the packet
    inet_pton(AF_INET, LOCAL_IP, &local_ip);
    inet_pton(AF_INET, DEST_IP, &dest_ip);

    packet.version = 4;
    packet.ihl = 5;
    packet.protocol = 17;
    packet.ttl = 64;
    packet.total_len = htons(61);
    packet.source_ip = local_ip;
    packet.dest_ip = dest_ip;
    packet.source_port = htons(LOCAL_PORT);
    packet.dest_port = htons(DEST_PORT);
    packet.payload_len = htons(PAYLOAD_SIZE_LEN(payload) + 8);

    memcpy(packet.payload, payload, PAYLOAD_SIZE_LEN(payload));

    // send the packet
    ret = sendto(
        sockfd, 
        &packet, 
        sizeof(struct udp_core_raw_packet), 
        0, 
        (struct sockaddr*)&socket_address, 
        sizeof(socket_address)
    );

    if (ret < 0) 
    {
        perror("packet send failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("packet sent successfully on interface %s\n", INTERFACE_NAME);

    // clean up
    close(sockfd);
    return 0;
}
