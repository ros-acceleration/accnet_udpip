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

char payload[MAX_PAYLOAD_SIZE];

int main() 
{
    int sockfd;
    ssize_t ret;
    socklen_t destination_len;
    struct sockaddr_in destination;
    
    char source_ip[INET_ADDRSTRLEN];
    
    // create a raw socket using AF_PACKET
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (sockfd < 0) 
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // set up the sockaddr structure
    memset(&destination, 0, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_port = htons(DEST_PORT);
    inet_pton(AF_INET, DEST_IP, &(destination.sin_addr));

    destination_len = sizeof(struct sockaddr);

    // copy payload to structure
    strncpy(payload, PAYLOAD, PAYLOAD_SIZE_LEN(PAYLOAD) + 1);

    // send packet
    ret = sendto(sockfd, payload, PAYLOAD_SIZE_LEN(PAYLOAD) + 1, 0, (struct sockaddr *)&destination, destination_len);
        
    if (ret < 0) 
    {
        perror("packet send failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("sent packet of size %ld bytes \n", ret);

    inet_ntop(AF_INET, &(destination.sin_addr.s_addr), source_ip, INET_ADDRSTRLEN);

    printf(" > packet destination IP: %s\n", source_ip);
    printf(" > packet destination port: %d\n", ntohs(destination.sin_port));
    printf(" > packet payload: %s\n", payload);

    // clean up
    close(sockfd);
    return 0;
}

