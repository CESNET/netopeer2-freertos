#ifndef _NETDB_H
#define _NETDB_H

#include "emb_posix_net_db.h"

/*
 * Constants for getnameinfo()
 */
#define    NI_MAXHOST  1025
#define    NI_MAXSERV  32

struct hostent *gethostbyname(const char *name);

int gethostbyname_r(const char *name, struct hostent *ret, char *buf, size_t buflen, struct hostent **result,
        int *h_errnop);

void freeaddrinfo(struct addrinfo *ai);

int getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res);

const char *gai_strerror(int ecode);

#endif /* _NETDB_H */
