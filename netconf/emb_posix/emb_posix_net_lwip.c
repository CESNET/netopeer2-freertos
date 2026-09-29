/**
 * @file emb_posix_net_lwip.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief embedded POSIX library, lwIP network backend
 *
 * The only module calling the lwIP API. Implements the emb_posix_net.h
 * contract and the POSIX socket API on top of it.
 *
 * @copyright
 * Copyright (c) 2026 CESNET, z.s.p.o.
 * Copyright (c) 2026, B&R Industrial Automation GmbH
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#define _GNU_SOURCE

#include "emb_posix_net.h"

#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include "lwip/if_api.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "emb_posix_internal.h"

int
embp_net_close(embp_net_sock sock)
{
    return lwip_close(sock);
}

ssize_t
embp_net_read(embp_net_sock sock, void *buf, size_t count)
{
    return lwip_read(sock, buf, count);
}

ssize_t
embp_net_write(embp_net_sock sock, const void *buf, size_t count)
{
    return lwip_write(sock, buf, count);
}

ssize_t
embp_net_readv(embp_net_sock sock, const struct iovec *iov, int iovcnt)
{
    return lwip_readv(sock, iov, iovcnt);
}

ssize_t
embp_net_writev(embp_net_sock sock, const struct iovec *iov, int iovcnt)
{
    return lwip_writev(sock, iov, iovcnt);
}

int
embp_net_ioctl(embp_net_sock sock, long cmd, void *argp)
{
    return lwip_ioctl(sock, cmd, argp);
}

int
embp_net_fcntl(embp_net_sock sock, int cmd, int val)
{
    return lwip_fcntl(sock, cmd, val);
}

int
embp_net_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    int rc, pollinout_timeout = 0;
    nfds_t i;

    /* FDs -> sockets */
    for (i = 0; i < nfds; ++i) {
        fds[i].fd = fdp[fds[i].fd].net.sock;

        if ((fds[i].events & POLLIN) && (fds[i].events & POLLOUT) && timeout) {
            /* BUG lwip always reports POLLOUT ready without any underlying changes which causes premature poll returns,
             * do not poll for the event */
            pollinout_timeout = 1;
            fds[i].events &= ~POLLOUT;
        }
    }

    /* poll */
    rc = lwip_poll(fds, nfds, timeout);

    if (pollinout_timeout && (rc > -1)) {
        /* generate POLLOUT on all FDs */
        for (i = 0; i < nfds; ++i) {
            fds[i].events |= POLLOUT;
            fds[i].revents |= POLLOUT;
        }
        rc = nfds;
    }

    /* sockets -> FDs */
    for (i = 0; i < nfds; ++i) {
        fds[i].fd = fdp_sock2fdp(fds[i].fd);
    }

    return rc;
}

/* sys/socket.h
 */

ssize_t
recv(int fd, void *mem, size_t len, int flags)
{
    ssize_t rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = lwip_recv(fdp[fd].net.sock, mem, len, flags);
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOTSOCK;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

ssize_t
recvfrom(int fd, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    ssize_t rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = lwip_recvfrom(fdp[fd].net.sock, mem, len, flags, from, fromlen);
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOTSOCK;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

ssize_t
recvmsg(int fd, struct msghdr *message, int flags)
{
    ssize_t rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = lwip_recvmsg(fdp[fd].net.sock, message, flags);
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOTSOCK;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

ssize_t
send(int fd, const void *dataptr, size_t size, int flags)
{
    ssize_t rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = lwip_send(fdp[fd].net.sock, dataptr, size, flags);
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOTSOCK;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

ssize_t
sendmsg(int fd, const struct msghdr *message, int flags)
{
    ssize_t rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = lwip_sendmsg(fdp[fd].net.sock, message, flags);
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOTSOCK;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

ssize_t
sendto(int fd, const void *dataptr, size_t size, int flags, const struct sockaddr *to, socklen_t tolen)
{
    ssize_t rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = lwip_sendto(fdp[fd].net.sock, dataptr, size, flags, to, tolen);
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOTSOCK;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

int
socket(int domain, int type, int protocol)
{
    int fd;

    fd = lwip_socket(domain, type, protocol);

    if (fd > -1) {
        fd = fdp_new_net(fd);
    }
    return fd;
}

int
connect(int fd, const struct sockaddr *name, socklen_t namelen)
{
    int rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = lwip_connect(fdp[fd].net.sock, name, namelen);
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOTSOCK;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

int
listen(int fd, int backlog)
{
    int rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = lwip_listen(fdp[fd].net.sock, backlog);
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOTSOCK;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

int
accept(int fd, struct sockaddr *addr, socklen_t *addrlen)
{
    int fd2 = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        fd2 = lwip_accept(fdp[fd].net.sock, addr, addrlen);
        if (fd2 > -1) {
            /* sock -> fd */
            fd2 = fdp_new_net(fd2);
        }
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOTSOCK;
        fd2 = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        fd2 = -1;
        break;
    }

    return fd2;
}

int
bind(int fd, const struct sockaddr *name, socklen_t namelen)
{
    int rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = lwip_bind(fdp[fd].net.sock, name, namelen);
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOTSOCK;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

int
shutdown(int fd, int how)
{
    int rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = lwip_shutdown(fdp[fd].net.sock, how);
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOTSOCK;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

int
getpeername(int fd, struct sockaddr *name, socklen_t *namelen)
{
    int rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = lwip_getpeername(fdp[fd].net.sock, name, namelen);
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOTSOCK;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

int
getsockname(int fd, struct sockaddr *name, socklen_t *namelen)
{
    int rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = lwip_getsockname(fdp[fd].net.sock, name, namelen);
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOTSOCK;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

int
getsockopt(int fd, int level, int optname, void *optval, socklen_t *optlen)
{
    int rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = lwip_getsockopt(fdp[fd].net.sock, level, optname, optval, optlen);
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOTSOCK;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

int
setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
    int rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = lwip_setsockopt(fdp[fd].net.sock, level, optname, optval, optlen);
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOTSOCK;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

int
socketpair(int domain, int type, int protocol, int sv[2])
{
    errno = EAFNOSUPPORT;
    return -1;
}

/* netdb.h
 */

struct hostent *
gethostbyname(const char *name)
{
    return lwip_gethostbyname(name);
}

int
gethostbyname_r(const char *name, struct hostent *ret, char *buf, size_t buflen, struct hostent **result,
        int *h_errnop)
{
    return lwip_gethostbyname_r(name, ret, buf, buflen, result, h_errnop);
}

void
freeaddrinfo(struct addrinfo *ai)
{
    lwip_freeaddrinfo(ai);
}

int
getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res)
{
    return lwip_getaddrinfo(nodename, servname, hints, res);
}

const char *
gai_strerror(int ecode)
{
    switch (ecode) {
    case 0:
        return "no error";
    case EAI_NONAME:
        return "name or service is not known";
    case EAI_FAIL:
        return "non-recoverable failure in name resolution";
    case EAI_FAMILY:
        return "ai_family not supported";
    case EAI_SERVICE:
        return "service not supported for ai_socktype";
    case EAI_MEMORY:
        return "memory allocation failure";
    default:
        return "unknown/invalid error";
    }
}

/* if.h
 */

char *
if_indextoname(unsigned int ifindex, char *ifname)
{
    return lwip_if_indextoname(ifindex, ifname);
}

unsigned int
if_nametoindex(const char *ifname)
{
    return lwip_if_nametoindex(ifname);
}

/* arpa/inet.h
 */

const char *
inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    return lwip_inet_ntop(af, src, dst, size);
}

int
inet_pton(int af, const char *src, void *dst)
{
    return lwip_inet_pton(af, src, dst);
}

/* sys/select.h
 */

int
select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout)
{
    int fd, s, rc;
    fd_set rset, wset, eset;

    FD_ZERO(&rset);
    FD_ZERO(&wset);
    FD_ZERO(&eset);

    /* check and create specific fd_set */
    for (fd = 0; fd < maxfdp1; ++fd) {
        if (FD_ISSET(fd, readset)) {
            if (FDP_TYPE(fd) == FDP_NET) {
                FD_SET(fdp[fd].net.sock, &rset);
            } else {
                errno = ENOSYS;
                return -1;
            }
        }
        if (FD_ISSET(fd, writeset)) {
            if (FDP_TYPE(fd) == FDP_NET) {
                FD_SET(fdp[fd].net.sock, &wset);
            } else {
                errno = ENOSYS;
                return -1;
            }
        }
        if (FD_ISSET(fd, exceptset)) {
            if (FDP_TYPE(fd) == FDP_NET) {
                FD_SET(fdp[fd].net.sock, &eset);
            } else {
                errno = ENOSYS;
                return -1;
            }
        }
    }

    if ((rc = lwip_select(maxfdp1, &rset, &wset, &eset, timeout)) < 0) {
        return rc;
    }

    /* move fd_set back to the generic FDP */
    FD_ZERO(readset);
    FD_ZERO(writeset);
    FD_ZERO(exceptset);
    for (s = 0; s < maxfdp1; ++s) {
        if (FD_ISSET(s, &rset)) {
            fd = fdp_sock2fdp(s);
            FD_SET(fd, readset);
        }
        if (FD_ISSET(s, &wset)) {
            fd = fdp_sock2fdp(s);
            FD_SET(fd, writeset);
        }
        if (FD_ISSET(s, &eset)) {
            fd = fdp_sock2fdp(s);
            FD_SET(fd, exceptset);
        }
    }

    return rc;
}
