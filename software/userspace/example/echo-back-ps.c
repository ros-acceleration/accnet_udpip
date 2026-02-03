#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PAYLOAD_MAX_LEN 1500

int main(int argc, char** argv) 
{
    int sockfd;
    int recvd;
    short port;
    socklen_t addrlen;
    struct sockaddr_in src_addr, dst_addr;
    char payload[PAYLOAD_MAX_LEN];

    if (argc < 3)
    {
        printf("Error: missing argument. \nUsage: %s <ip> <port> \n", argv[0]);
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[2]);

    // Clear the payload.
    memset(payload, 0, sizeof(payload));

    // Creating a UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set source address and bind the socket to the source IP and port
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.sin_family = AF_INET;
    src_addr.sin_addr.s_addr = inet_addr(argv[1]);
    src_addr.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) 
    {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Echo-back on IP %s port %d started... \n", argv[1], port);

    // Infinite loop to send the payload back
    while (1) 
    {
        addrlen = sizeof(src_addr);
        memset(&src_addr, 0, sizeof(src_addr));

        recvd = recvfrom(sockfd, payload, sizeof(payload), 0, (struct sockaddr *) &src_addr, &addrlen);

        if (recvd < 0)
        {
            // receive failed. try again!
            perror("Error during receive");
            continue;
        }

        memset(&dst_addr, 0, sizeof(dst_addr));

        dst_addr.sin_family = AF_INET;
        dst_addr.sin_addr.s_addr = src_addr.sin_addr.s_addr;
        dst_addr.sin_port = src_addr.sin_port;

        if (sendto(sockfd, payload, recvd, 0, (const struct sockaddr *) &dst_addr, sizeof(dst_addr)) < 0) 
        {
            // send failed. try again!
            perror("Error during send");
            continue;
        }

    }

    close(sockfd);
    printf("Socket on src port %d closed.\n", port);

    return 0;
}
