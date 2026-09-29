#ifndef _SYS_UIO_H_
#define _SYS_UIO_H_

#include "emb_posix_net_sockets.h"

ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t writev(int fd, const struct iovec *iov, int iovcn);

#endif /* _SYS_UIO_H_ */
