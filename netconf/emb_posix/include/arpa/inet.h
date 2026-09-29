#ifndef _ARPA_INET_H
#define _ARPA_INET_H

#include <sys/select.h>

#include "emb_posix_net_sockets.h"

int inet_pton(int af, const char *cp, void *buf);

const char *inet_ntop(int af, const void *cp, char *buf, socklen_t len);

#endif /* _ARPA_INET_H */
