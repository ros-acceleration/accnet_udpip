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
    socklen_t sender_len;
    struct sockaddr_in socket_addr;
    struct sockaddr_in sender;
    
    char source_ip[INET_ADDRSTRLEN];
    
    // create a raw socket using AF_PACKET
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (sockfd < 0) 
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // set up the sockaddr_ll structure
    memset(&socket_addr, 0, sizeof(socket_addr));
    socket_addr.sin_family = AF_INET;
    socket_addr.sin_port = htons(DEST_PORT);
    socket_addr.sin_addr.s_addr = INADDR_ANY;

    // bind the socket to the specified interface
    if (bind(sockfd, (struct sockaddr *)&socket_addr, sizeof(socket_addr)) < 0) 
    {
        perror("binding socket failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    sender_len = sizeof(struct sockaddr);

    while(1)
    {
        ret = recvfrom(sockfd, payload, MAX_PAYLOAD_SIZE, 0, (struct sockaddr *)&sender, &sender_len);
        
        if (ret < 0) 
        {
            perror("packet receive failed");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        printf("received packet of size %ld bytes \n", ret);

        inet_ntop(AF_INET, &(sender.sin_addr.s_addr), source_ip, INET_ADDRSTRLEN);

        printf(" > packet source IP: %s\n", source_ip);
        printf(" > packet source port: %d\n", ntohs(sender.sin_port));
        printf(" > packet payload: %s\n", payload);
    }

    // clean up
    close(sockfd);
    return 0;
}

