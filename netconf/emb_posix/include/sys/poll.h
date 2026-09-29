#ifndef _SYS_POLL_H
#define _SYS_POLL_H

#include <sys/select.h>

#include "emb_posix_net_sockets.h"

int poll(struct pollfd *fds, nfds_t nfds, int timeout);

#endif /* _SYS_POLL_H */
