#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "config.h"

#define PAYLOAD_MAX_LEN 1500

struct udp_core_raw_packet packet;

int main() 
{
    int sockfd;
    int ret;
    int sourceip, destip;
    short sourceport, destport;
    struct ifreq ifr;
    struct sockaddr_ll socket_address;

    // Creating a RAW socket
    if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0) 
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Clear and set up the ifreq structure to specify the interface
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, INTERFACE_NAME, IFNAMSIZ-1);

    // Get the interface index
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) 
    {
        perror("ioctl SIOCGIFINDEX failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Set up the sockaddr_ll structure
    memset(&socket_address, 0, sizeof(socket_address));
    socket_address.sll_family = AF_PACKET;
    socket_address.sll_protocol = htons(ETH_P_IP);
    socket_address.sll_ifindex = ifr.ifr_ifindex;

    if (bind(sockfd, (const struct sockaddr *)&socket_address, sizeof(socket_address)) < 0) 
    {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Echo-back on interface %s started... \n", INTERFACE_NAME);

    // Infinite loop to send the payload back
    while (1) 
    {
        ret = recvfrom(sockfd, &packet, sizeof(struct udp_core_raw_packet), 0, NULL, NULL);

        if (ret < 0)
        {
            // Receive failed. try again!
            perror("Error during receive");
            continue;
        }

        sourceip = packet.source_ip;
        destip = packet.dest_ip;
        sourceport = packet.source_port;
        destport = packet.dest_port;

        printf("Received a packet. Send it back! \n");

        // Loop back!
        packet.source_ip = destip;
        packet.dest_ip = sourceip;
        packet.source_port = destport;
        packet.dest_port = sourceport;

        ret = sendto(sockfd, &packet, sizeof(struct udp_core_raw_packet), 0, (struct sockaddr*)&socket_address, sizeof(socket_address));

        if (ret < 0) 
        {
            // Send failed. try again!
            perror("Error during send");
            continue;
        }

    }

    close(sockfd);
    return 0;
}
