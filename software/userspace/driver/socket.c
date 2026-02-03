/*
 * Userspace socket abstraction for UDP Ethernet Stack in FPGA
 *
 * Copyright (C) Accelerat S.r.l.
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/epoll.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <unistd.h>

#include "socket.h"
#include "udriver.h"

/****************************************************************************
* Macros
****************************************************************************/

#define UNUSED(value) (void)value
#define SEC_TO_USEC(sec) (sec * 1000000L)
#define MSEC_TO_USEC(msec) (msec * 1000L)
#define TVP_TO_USEC(tvp) (SEC_TO_USEC(tvp->tv_sec) + tvp->tv_usec)
#define TV_TO_USEC(tv) (SEC_TO_USEC(tv.tv_sec) + tv.tv_usec)

/****************************************************************************
* Private struct: definitions & declarations
****************************************************************************/

static int initialized = 0;

enum udriver_socket_status_t 
{
    NOT_ASSIGNED,
    INITIALIZED,
    BOUND,
};

struct udriver_socket_t
{
    uint32_t multicast;
    uint32_t src_ip;
    uint16_t src_port;
    uint32_t dest_ip;
    uint16_t dest_port;
};

struct udriver_socket_id_t
{
    int epfd;
    enum udriver_socket_status_t status;
    struct udriver_socket_t* socket_ptr;
};

static struct udriver_socket_id_t socket_fds[MAX_UDP_PORTS];

struct epoll_entry_t 
{
    int sockfd;
    void *data;
    uint32_t events;
};

struct epoll_fd_t 
{
    struct epoll_entry_t entries[MAX_EPOLL_FDS];
    int size;
};

static struct epoll_fd_t* epoll_instances[MAX_EPOLL_FDS];

/**
 * Temporary buffer used to store incoming data when dealing with sendmsg
 * syscalls.
 */
static uint64_t temp_buffer[PACKET_PAYL_SIZE_MAX_LEN];

/**
 * Array of bytes used to configure the device.
 */

const uint8_t local_mac[ETH_ALEN] = LOCAL_MAC;
const uint8_t local_ip[INET_ALEN] = LOCAL_IP;
const uint8_t subnet_mask[INET_ALEN] = LOCAL_SUBNET;
const uint8_t gw_ip[INET_ALEN] = GW_IP;

/**
 * Typedefs for libc functions.
 */
typedef int (*close_func_t)(int);

/****************************************************************************
* Private functions: declarations
****************************************************************************/

static void fds_init(void);
static int fds_get_free_fd(void);
static int fd_create(void);
static void nsleep(uint64_t nanoseconds);
static inline void __trace(const char* func, const char* fmt, ...);
static inline void __log(const char* fmt, ...);

/****************************************************************************
* Public functions: definitions
****************************************************************************/

#ifdef AUTO_INIT
__attribute__((constructor)) 
#endif
void lib_init()
{
    int retval;

    __trace(__func__, NULL);

    fds_init();
    
    retval = udriver_initialize(
      local_mac, 
      local_ip, 
      subnet_mask, 
      gw_ip, 
      LOCAL_PORT_MIN, 
      LOCAL_PORT_MAX);

    if (retval < 0)
    {
        __log("init failed. Abort. \n");
        abort();
    }
}

int socket(int domain, int type, int protocol)
{
    int sockfd;

    UNUSED(protocol);

    __trace(__func__, "%d, %d, %d", domain, type, protocol);

    #ifndef AUTO_INIT
    if (initialized == 0)
    {
        lib_init();
        initialized = 1;
    }
    #endif

    if (domain != AF_INET && type != SOCK_DGRAM)
    {
        // IPv6 and TCP over IPv4 are not supported
        __log("socket creation failed - Invalid domain or type. \n");
        errno = EINVAL;
        return -1;
    }

    sockfd = fd_create();

    if (sockfd < 0) 
    {
        __log("socket creation failed - Unable to create new fd. \n");
        errno = ENOMEM;
        return -1;
    }

    __log("socket created: %d \n", sockfd);

    return sockfd;
}

int shutdown(int sockfd, int how)
{
    struct udriver_socket_t* socket_ptr;

    __trace(__func__, "%d, %d", sockfd, how);

    if (socket_fds[sockfd].status == NOT_ASSIGNED)
    {
        __log("shutdown failed - invalid fd. \n");
        errno = EBADF;
        return -1;
    }

    socket_ptr = socket_fds[sockfd].socket_ptr;

    if (how != SHUT_WR && how != SHUT_RD && how != SHUT_RDWR)
    {
        __log("shutdown failed - invalid how. \n");
        errno = EINVAL;
        return -1;
    }

    if (how == SHUT_WR || how == SHUT_RDWR)
    {
        socket_ptr->dest_ip = 0;
        socket_ptr->dest_port = 0;
    }
    else if (how == SHUT_RD || how == SHUT_RDWR)
    {
        if (socket_fds[sockfd].status == BOUND) 
        {
            udriver_set_socket_status(socket_ptr->src_port, UDRIVER_SOCKET_CLOSED);
        }
    }

    if (socket_fds[sockfd].epfd != -1)
    {
        epoll_ctl(socket_fds[sockfd].epfd, EPOLL_CTL_DEL, sockfd, NULL);
    }

    return 0;
}

int close(int fd)
{
    struct udriver_socket_t* socket_ptr;
    static close_func_t libc_close = NULL;

    __trace(__func__, "%d", fd);

    if (libc_close == NULL)
    {
        libc_close = (close_func_t) dlsym(RTLD_NEXT, "close");

        if (!libc_close)
        {
            __log("close fails to resolve libc close: %s\n", dlerror());
            errno = ENOSYS;
            return -1;
        }
    }

    if (socket_fds[fd].status == NOT_ASSIGNED)
    {
        if (fcntl(fd, F_GETFD) != -1)
        {
            // forward it to libc close
            return libc_close(fd);
        }
        else
        {
            __log("close failed - invalid fd. \n");
            errno = EBADF;
            return -1;
        }
    }

    shutdown(fd, SHUT_RDWR);

    socket_ptr = socket_fds[fd].socket_ptr;

    free(socket_ptr);

    socket_fds[fd].status = NOT_ASSIGNED;
    socket_fds[fd].socket_ptr = NULL;

    return libc_close(fd);
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    struct udriver_socket_t* socket_ptr;
    const struct sockaddr_in* addr_in;
    uint32_t ip;
    uint16_t port;
    
    __trace(__func__, "%d, %p, %d", sockfd, addr, addrlen);

    if (socket_fds[sockfd].status == NOT_ASSIGNED)
    {
        __log("bind failed - Invalid fd. \n");
        errno = EBADF;
        return -1;
    }
    
    if (socket_fds[sockfd].status == BOUND)
    {
        __log("bind failed - fd already bound. \n");
        errno = EINVAL;
        return -1; // At the current stage re-binding is not supported.
    }

    if (addr->sa_family != AF_INET || addrlen != sizeof(struct sockaddr_in))
    {
        __log("bind failed - AF not supported. \n");
        errno = EAFNOSUPPORT;
        return -1; // IPv6 and other families are not supported.
    }

    socket_ptr = socket_fds[sockfd].socket_ptr;
    addr_in = (const struct sockaddr_in*) addr;
    
    ip = ntohl(addr_in->sin_addr.s_addr);
    port = ntohs(addr_in->sin_port);

    if (ip == INADDR_ANY)
        ip = udriver_get_local_ip();

    /* Asking for an automatic port selection. Upper range is used for these cases */
    if (port == 0)
        port = udriver_get_port_range_high() - 1 - sockfd;

    socket_fds[sockfd].status = BOUND;
    socket_ptr->src_ip = ip;
    socket_ptr->src_port = port;

    __log("bind succeed for sock %d - port %d \n", sockfd, port);

    udriver_set_socket_status(port, UDRIVER_SOCKET_OPEN);

    return 0;
}

int listen(int sockfd, int backlog)
{
    UNUSED(sockfd);
    UNUSED(backlog);

    __trace(__func__, "%d, %d", sockfd, backlog);

    // TCP sockets are not supported, therefore listen cannot be used.

    errno = EOPNOTSUPP;
    return -1;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    const struct sockaddr_in* addr_in;
    struct udriver_socket_t* socket_ptr;

    __trace(__func__, "%d, %p, %d", sockfd, addr, addrlen);

    if (addr->sa_family != AF_INET || addrlen != sizeof(struct sockaddr_in))
    {
        __log("connect failed - AF not supported. \n");
        errno = EAFNOSUPPORT;
        return -1; // IPv6 and other families are not supported.
    }

    if (socket_fds[sockfd].status == NOT_ASSIGNED)
    {
        __log("connect failed - Invalid fd. \n");
        errno = EBADF;
        return -1;
    }

    socket_ptr = socket_fds[sockfd].socket_ptr;
    addr_in = (const struct sockaddr_in *) addr;

    socket_ptr->dest_ip = ntohl(addr_in->sin_addr.s_addr);
    socket_ptr->dest_port = ntohs(addr_in->sin_port);

    return 0;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    UNUSED(sockfd);
    UNUSED(addr);
    UNUSED(addrlen);

    __trace(__func__, "%d, %p, %p", sockfd, addr, addrlen);

    // TCP sockets are not supported, therefore listen cannot be used.
    
    errno = EOPNOTSUPP;
    return -1; // TCP sockets are not supported, therefore accept cannot be used.
}

int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    struct sockaddr_in* addr_in;
    struct udriver_socket_t* socket_ptr;

    __trace(__func__, "%d, %p, %p", sockfd, addr, addrlen);
    
    if (socket_fds[sockfd].status == NOT_ASSIGNED)
    {
        __log("Getsockname failed - Invalid fd. \n");
        errno = EBADF;
        return -1;
    }

    if (addrlen)
        *addrlen = sizeof(struct sockaddr_in);

    addr_in = (struct sockaddr_in*) addr;
    socket_ptr = socket_fds[sockfd].socket_ptr;
        
    addr_in->sin_family = AF_INET;
    addr_in->sin_addr.s_addr = htonl(socket_ptr->src_ip);
    addr_in->sin_port = htons(socket_ptr->src_port);

    return 0;
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
    unsigned* optval_int_ptr;
    
    UNUSED(sockfd);
    UNUSED(optlen);

    __trace(__func__, "%d, %d, %d, %p, %p", sockfd, level, optname, optval, optlen);

    optval_int_ptr = (unsigned*) optval;
    
    if (level == SOL_SOCKET && optname == SO_RCVBUF) 
    {
        *optval_int_ptr = 65536;
    } 
    else if (level == SOL_SOCKET && optname == SO_SNDBUF) 
    {
        *optval_int_ptr = 65536;
    }
  
    return 0;
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    UNUSED(level);
    UNUSED(optname);
    UNUSED(optval);
    UNUSED(optlen);

    __trace(__func__, "%d, %d, %d, %p, %d", sockfd, level, optname, optval, optlen);

    if (socket_fds[sockfd].status == NOT_ASSIGNED)
    {
        __log("setsockopt failed - invalid fd. \n");
        errno = EBADF;
        return -1;
    }

    if (level == IPPROTO_IP && optname == IP_MULTICAST_IF)
    {
        socket_fds[sockfd].socket_ptr->multicast = 1;
    }

    return 0;
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
    struct sockaddr_in* addr;
    ssize_t received;
    struct udp_packet rx_udp_packet;
    struct udriver_socket_t* socket_ptr;
    uint16_t port;

    UNUSED(flags);

    __trace(__func__, "%d, %p, %d, %d, %p, %p", sockfd, buf, len, flags, src_addr, addrlen);

    if (socket_fds[sockfd].status != BOUND)
    {
        __log("recvfrom failed - socket not bound. \n");
        errno = EINVAL;
        return -1;
    }

    socket_ptr = socket_fds[sockfd].socket_ptr;
    port = socket_ptr->src_port;
    rx_udp_packet.payload = (uint64_t*) buf;

    if (socket_ptr->multicast == 1)
    {
        return 0;
    }

    do 
    {
        received = udriver_recv(&rx_udp_packet, port);
        
        if (received == 0)
            nsleep(1); // Reduce pressure on CPU when non-blocking reads are issued
    } 
    while(received == 0);
    
    received = (rx_udp_packet.payload_size_bytes < len) ? 
        rx_udp_packet.payload_size_bytes : len;

    if (addr != NULL)
    {
        addr = (struct sockaddr_in*) src_addr;
        addr->sin_family = AF_INET;
        addr->sin_port = htons(rx_udp_packet.source_port);
        addr->sin_addr.s_addr = htonl(rx_udp_packet.source_ip);
    }

    if (addrlen != NULL)
        *addrlen = sizeof(struct sockaddr_in);
    
    return received;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    socklen_t addrlen;
    struct sockaddr_in addr;

    __trace(__func__, "%d, %p, %d, %d", sockfd, buf, len, flags);

    return recvfrom(sockfd, buf, len, flags, (struct sockaddr*)&addr, &addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
{
    socklen_t addrlen;
    struct sockaddr_in addr;
    unsigned int received;

    __trace(__func__, "%d, %p, %d", sockfd, msg, flags);

    received = recvfrom(
        sockfd, 
        msg->msg_iov[0].iov_base, 
        msg->msg_iov[0].iov_len, 
        flags,
        (struct sockaddr*)&addr,
        &addrlen
    );

    msg->msg_namelen = addrlen;
    msg->msg_iovlen = 1;
    msg->msg_iov[0].iov_len = received;

    memcpy(msg->msg_name, (void*)&addr, sizeof(struct sockaddr_in));
    
    return received;
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
    int sentb;
    int retval;
    struct sockaddr_in* sockaddr;
    struct sockaddr_in ephemeral;
    struct udp_packet tx_udp_packet;
    struct udriver_socket_t* socket_ptr;

    UNUSED(flags);

    __trace(__func__, "%d, %p, %d, %d, %p, %d", sockfd, buf, len, flags, dest_addr, addrlen);

    if (socket_fds[sockfd].status == NOT_ASSIGNED)
    {
        __log("sendto failed - invalid fd. \n");
        errno = EBADF;
        return -1;
    }

    /* Socket not bound. Assign an ephemeral port */
    if (socket_fds[sockfd].status != BOUND)
    {
        memset(&ephemeral, 0, sizeof(struct sockaddr_in));
        ephemeral.sin_family = AF_INET;
        ephemeral.sin_addr.s_addr = INADDR_ANY;
        ephemeral.sin_port = 0;

        retval = bind(sockfd, (struct sockaddr *)&ephemeral, sizeof(ephemeral));

        if (retval < 0)
        {
            __log("sendto failed - unable to bind. \n");
            errno = EINVAL;
            return -1;
        }

    }

    if (addrlen != sizeof(struct sockaddr_in))
    {
        __log("sendto failed - AF not supported. \n");
        errno = EAFNOSUPPORT;
        return -1;
    }

    sockaddr = (struct sockaddr_in*) dest_addr;
    socket_ptr = socket_fds[sockfd].socket_ptr;

    tx_udp_packet.payload_size_bytes = len;
    tx_udp_packet.source_ip = socket_ptr->src_ip;
    tx_udp_packet.source_port = socket_ptr->src_port;
    tx_udp_packet.dest_ip = htonl(sockaddr->sin_addr.s_addr);
    tx_udp_packet.dest_port = htons(sockaddr->sin_port);
    tx_udp_packet.payload = (uint64_t*) buf;

    do
    {
        sentb = udriver_send(&tx_udp_packet);

        if (sentb < 0)
            nsleep(1);
    }
    while (sentb <= 0);
    
    return sentb;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    int sentb;
    struct sockaddr_in sockaddr;
    struct udriver_socket_t* socket_ptr;

    __trace(__func__, "%d, %p, %d, %d", sockfd, buf, len, flags);

    if (socket_fds[sockfd].status == NOT_ASSIGNED)
    {
        __log("send failed - invalid fd. \n");
        errno = EBADF;
        return -1;
    }

    socket_ptr = socket_fds[sockfd].socket_ptr;

    // 'send' syscall makes sense iff socket was "connected"
    if (socket_ptr->dest_ip == 0)
    {
        __log("send failed - socket not connected. \n");
        errno = ENOTCONN;
        return -1;
    }

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = htonl(socket_ptr->dest_ip);
    sockaddr.sin_port = htonl(socket_ptr->dest_port);

    sentb = sendto(
        sockfd, 
        buf, 
        len, 
        flags, 
        (struct sockaddr*)&sockaddr, 
        sizeof(struct sockaddr_in)
    );
        
    return sentb;    
}


ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
    int sentb;
    unsigned total_len, iov_len;

    __trace(__func__, "%d, %p, %d", sockfd, msg, flags);

    if (msg->msg_namelen != sizeof(struct sockaddr_in))
    {
        __log("sendmsg failed - AF not supported. \n");
        errno = EAFNOSUPPORT;
        return -1;
    }

    if (socket_fds[sockfd].status == NOT_ASSIGNED)
    {
        __log("sendmsg failed - invalid fd. \n");
        errno = EBADF;
        return -1;
    }

    total_len = 0;

    for (unsigned int i = 0; i < msg->msg_iovlen; i++) 
    {
        iov_len = msg->msg_iov[i].iov_len;
        
        if ((total_len + iov_len) > (BUF_ELEM_MAX_SIZE_BYTES - PACKET_HDR_SIZE_BYTES)) 
        {
            __log("sendmsg failed - message too long. \n");
            errno = EMSGSIZE;
            return -1; // We cannot send that amount of data.
        }

        memcpy((void*) temp_buffer + total_len, msg->msg_iov[i].iov_base, iov_len);
        total_len += iov_len;
    }

    sentb = sendto(
        sockfd, 
        temp_buffer, 
        total_len, 
        flags, 
        msg->msg_name, 
        msg->msg_namelen
    );

    return sentb;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    int flag;
    int count;
    long timeout_us;
    long elapsed_us;
    struct timeval start, now;
    struct udriver_socket_t* socket_ptr;

    UNUSED(writefds);
    UNUSED(exceptfds);

    __trace(__func__, "%d, %p, %p, %p, %p", nfds, readfds, writefds, exceptfds, timeout);
    
    if (readfds == NULL)
    {
        return -1;
    }

    FD_ZERO(readfds);

    if (timeout) 
    {
        timeout_us = TVP_TO_USEC(timeout);
        gettimeofday(&start, NULL);
    }

    while (1)
    {
        count = 0;

        for (int i = 0; i < nfds; i++) 
        {
            if (socket_fds[i].status != BOUND)
            {
                continue;
            }

            socket_ptr = socket_fds[i].socket_ptr;
            flag = udriver_probe_port(socket_ptr->src_port);

            if (flag) 
            {
                FD_SET(i, readfds);
                count++;
            }
        }

        if (count > 0)
        {
            return count;
        }

        if (timeout_us >= 0) 
        {
            gettimeofday(&now, NULL);
            elapsed_us = TV_TO_USEC(now) - TV_TO_USEC(start);
            
            if (elapsed_us >= timeout_us)
                return 0;  
        }
    }
}

int epoll_create1(int flags)
{
    UNUSED(flags);

    __trace(__func__, "%d", flags);

    return epoll_create(1);
}

int epoll_create(int size)
{
    int epfd;
    
    UNUSED(size);

    __trace(__func__, "%d", size);

    epfd = fds_get_free_fd();

    if (epfd < 0 || epfd >= MAX_EPOLL_FDS)
    {
        __log("epoll creation failed - unable to create new fd. \n");
        errno = ENOMEM;
        return -1;
    }

    epoll_instances[epfd] = (struct epoll_fd_t*) calloc(1, sizeof(struct epoll_fd_t));
    
    if (epoll_instances[epfd] == NULL)
    {
        __log("epoll creation failed - unable to allocate epoll struct. \n");
        errno = ENOMEM;
        return -1;
    }

    __log("epollfd created: %d \n", epfd);

    return epfd;
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
    struct epoll_fd_t* instance;

    __trace(__func__, "%d, %d, %d, %p", epfd, op, fd, event);

    if (epfd < 0 || epfd >= MAX_EPOLL_FDS || epoll_instances[epfd] == NULL)
    {
        __log("epoll ctl failed - invalid epoll fd. \n");
        errno = EBADF;
        return -1;
    }

    instance = epoll_instances[epfd];

    switch (op) 
    {
        case EPOLL_CTL_ADD:

            for (int i = 0; i < instance->size; i++) 
            {
                if (instance->entries[i].sockfd == fd)
                {
                    __log("epoll ctl failed - fd already added. \n");
                    errno = EBADF;
                    return -1;  // Already added
                }
            }

            if (instance->size >= MAX_EPOLL_FDS)
            {
                __log("epoll ctl failed - epoll full. \n");
                errno = E2BIG;
                return -1;
            }

            socket_fds[fd].epfd = epfd;
            instance->entries[instance->size].sockfd = fd;
            instance->entries[instance->size].events = event->events;
            instance->entries[instance->size].data = event->data.ptr;
            instance->size++;
            break;

        case EPOLL_CTL_DEL:
            for (int i = 0; i < instance->size; ++i) 
            {
                if (instance->entries[i].sockfd == fd) 
                {
                    for (int j = i; j < instance->size - 1; ++j) 
                    {
                        instance->entries[j] = instance->entries[j + 1];
                    }

                    socket_fds[fd].epfd = -1;
                    instance->size--;
                    return 0;
                }
            }

            return -1;

        default:
            errno = ENOTSUP;
            __log("epoll ctl failed - op not supported. \n");
            return -1;
    }

    return 0;
}

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
    struct epoll_fd_t* instance;
    struct timeval start, now;
    long timeout_us;
    long elapsed_us;
    int nevents;

    __trace(__func__, "%d, %p, %d, %d", epfd, events, maxevents, timeout);

    if (epfd < 0 || epfd >= MAX_EPOLL_FDS || epoll_instances[epfd] == NULL)
    {
        __log("epoll wait failed - invalid epoll fd. \n");
        errno = EBADF;
        return -1;
    }

    instance = epoll_instances[epfd];

    if (timeout >= 0)
    {
        timeout_us = MSEC_TO_USEC(timeout);
        gettimeofday(&start, NULL);
    }

    while (1) 
    {
        nevents = 0;

        for (int i = 0; i < instance->size && nevents < maxevents; i++) 
        {
            int sockfd = instance->entries[i].sockfd;
            struct udriver_socket_t* socket_ptr;

            if (socket_fds[sockfd].status != BOUND)
                continue;

            socket_ptr = socket_fds[sockfd].socket_ptr;

            if (udriver_probe_port(socket_ptr->src_port)) 
            {
                __log("epoll_wait - received something on fd %d port %d \n", sockfd, socket_ptr->src_port);
                events[nevents].data.fd = sockfd;
                events[nevents].data.ptr = instance->entries[i].data;
                events[nevents].events = EPOLLIN;
                nevents++;
                __log("epoll_wait - events %d. \n", nevents);
            }
        }

        if (nevents > 0)
            return nevents;

        if (timeout == 0)
            return 0;

        if (timeout > 0) 
        {
            gettimeofday(&now, NULL);
            elapsed_us = TV_TO_USEC(now) - TV_TO_USEC(start);

            if (elapsed_us >= timeout_us)
                return 0;
        }

        nsleep(1000); // 1 microsecond sleep
    }
}


/****************************************************************************
* Private functions: definitions
****************************************************************************/

static void fds_init(void) 
{
    for (unsigned i = 0; i < MAX_UDP_PORTS; i++) 
    {
        socket_fds[i].status = NOT_ASSIGNED;
        socket_fds[i].socket_ptr = NULL;
    }
}

static int fds_get_free_fd(void) 
{
    int fd;

    /* Bookep a FD using a "dummy" one */
    fd = open("/dev/null", O_RDONLY);

    if (fd < 0)
        return -1;

    /* This should not happen, because all assigned fds are bookeeped */
    if (socket_fds[fd].status != NOT_ASSIGNED)
        return -1;

    return fd;
}

static int fd_create(void) 
{
    int sockfd;
    struct udriver_socket_t* socket_ptr;

    sockfd = fds_get_free_fd();

    if (sockfd == -1)
        return -1;

    socket_ptr = (struct udriver_socket_t*) malloc(sizeof(struct udriver_socket_t));

    if (socket_ptr == NULL)
        return -1;

    memset((void*)socket_ptr, 0, sizeof(struct udriver_socket_t));

    socket_fds[sockfd].epfd = -1;
    socket_fds[sockfd].status = INITIALIZED;
    socket_fds[sockfd].socket_ptr = socket_ptr;
    
    return sockfd;
}

/**
 * Suspends the execution of the calling thread until at least the time 
 * specified in nanoseconds has elapsed.
 */
static void nsleep(uint64_t nanoseconds)
{
    struct timespec duration;
    
    duration.tv_nsec = nanoseconds;
    duration.tv_sec = 0;

    nanosleep(&duration, NULL);
}

static inline void __trace(const char* func, const char* fmt, ...)
{
    #ifndef TRACE
    UNUSED(func);
    UNUSED(fmt);
    #else
    if (fmt == NULL)
    {
        fprintf(stdout, "[libsock.so] __trace %d -> %s() \n", gettid(), func);
    }
    else
    {
        fprintf(stdout, "[libsock.so] __trace %d -> %s(", gettid(), func);

        va_list args;
        va_start(args, fmt);
        vfprintf(stdout, fmt, args);
        va_end(args);

        fprintf(stdout, ")\n");
    }
    #endif
}

static inline void __log(const char* fmt, ...)
{
    #ifndef LOG
    UNUSED(fmt);
    #else
    va_list args;

    printf("[libsock.so] __log - ");
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    #endif
}