#ifndef _NET_IF_H
#define _NET_IF_H

#include "emb_posix_net_if.h"

char *if_indextoname(unsigned int ifindex, char *ifname);

unsigned int if_nametoindex(const char *ifname);

#endif /* _NET_IF_H */
