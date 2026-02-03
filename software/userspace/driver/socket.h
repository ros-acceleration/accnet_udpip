/*
 * Userspace socket abstraction for UDP Ethernet Stack in FPGA
 *
 * Copyright (C) Accelerat S.r.l.
 */

/****************************************************************************
* header guard
****************************************************************************/

#ifndef SOCKETLIB_H
#define SOCKETLIB_H

/****************************************************************************
* driver settings
****************************************************************************/

#define LOCAL_IP                {192, 168, 1, 128}
#define GW_IP                   {192, 168, 1, 2}
#define LOCAL_SUBNET            {255, 255, 255, 0}
#define LOCAL_MAC               {0x02, 0x00, 0x00, 0x00, 0x00, 0x00}

#define LOCAL_PORT_MIN          7400
#define LOCAL_PORT_MAX          7500

#define MAX_EPOLL_FDS           128
#define MAX_EPOLL_EVENTS        64

/****************************************************************************
* includes
****************************************************************************/

#include <sys/types.h>

/****************************************************************************
* type decls
****************************************************************************/

struct fd_set;

/****************************************************************************
* fun decls
****************************************************************************/

int socket(int domain, int type, int protocol);

int shutdown(int sockfd, int how);

int close(int sockfd);

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int listen(int sockfd, int backlog);

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

ssize_t recv(int sockfd, void *buf, size_t len, int flags);

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

ssize_t send(int sockfd, const void *buf, size_t len, int flags);

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);

int epoll_create(int size);

int epoll_create1(int flags);

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

#endif  // SOCKETLIB_H