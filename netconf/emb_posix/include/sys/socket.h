#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <sys/select.h>

#include "emb_posix_net_sockets.h"

/* User visible structure for SCM_CREDENTIALS message */
struct ucred
{
  pid_t pid;                    /* PID of sending process.  */
  uid_t uid;                    /* UID of sending process.  */
  gid_t gid;                    /* GID of sending process.  */
};

/* For setsockopt(2) */
#define SO_PEERCRED        17

ssize_t recv(int fd, void *mem, size_t len, int flags);

ssize_t recvfrom(int fd, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);

ssize_t recvmsg(int fd, struct msghdr *message, int flags);

ssize_t send(int fd, const void *dataptr, size_t size, int flags);

ssize_t sendmsg(int fd, const struct msghdr *message, int flags);

ssize_t sendto(int fd, const void *dataptr, size_t size, int flags, const struct sockaddr *to, socklen_t tolen);

int socket(int domain, int type, int protocol);

int connect(int fd, const struct sockaddr *name, socklen_t namelen);

int listen(int fd, int backlog);

int accept(int fd, struct sockaddr *addr, socklen_t *addrlen);

int bind(int fd, const struct sockaddr *name, socklen_t namelen);

int shutdown(int fd, int how);

int getpeername(int fd, struct sockaddr *name, socklen_t *namelen);

int getsockname(int fd, struct sockaddr *name, socklen_t *namelen);

int getsockopt(int fd, int level, int optname, void *optval, socklen_t *optlen);

int setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen);

int socketpair(int domain, int type, int protocol, int sv[2]);

#endif /* _SYS_SOCKET_H */
