/**
 * @file emb_posix_net.h
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief embedded POSIX library, network backend interface
 *
 * Internal contract implemented by exactly one backend module, so that the
 * network stack dependency can be replaced without touching the POSIX layer.
 * The build system picks the module via EMB_POSIX_NET_SOURCE and its
 * emb_posix_net_backend.h via EMB_POSIX_NET_INCLUDE_DIR, which also allows a
 * backend outside of this project. The backend implements the POSIX socket
 * API (sys/socket.h, netdb.h, net/if.h, arpa/inet.h, sys/select.h) as well.
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

#ifndef EMB_POSIX_NET_H_
#define EMB_POSIX_NET_H_

#include <poll.h>
#include <sys/types.h>
#include <sys/uio.h>

/* embp_net_sock */
#include "emb_posix_net_backend.h"

/**
 * @brief Close a socket.
 *
 * @param[in] sock Backend socket.
 * @return 0 on success, -1 on error.
 */
int embp_net_close(embp_net_sock sock);

/**
 * @brief Read from a socket.
 *
 * @param[in] sock Backend socket.
 * @param[out] buf Buffer to fill.
 * @param[in] count Number of bytes to read.
 * @return Number of bytes read, -1 on error.
 */
ssize_t embp_net_read(embp_net_sock sock, void *buf, size_t count);

/**
 * @brief Write into a socket.
 *
 * @param[in] sock Backend socket.
 * @param[in] buf Data to write.
 * @param[in] count Number of bytes to write.
 * @return Number of bytes written, -1 on error.
 */
ssize_t embp_net_write(embp_net_sock sock, const void *buf, size_t count);

/**
 * @brief Scattered read from a socket.
 *
 * @param[in] sock Backend socket.
 * @param[in] iov Buffers to fill.
 * @param[in] iovcnt Number of @p iov items.
 * @return Number of bytes read, -1 on error.
 */
ssize_t embp_net_readv(embp_net_sock sock, const struct iovec *iov, int iovcnt);

/**
 * @brief Gathered write into a socket.
 *
 * @param[in] sock Backend socket.
 * @param[in] iov Buffers to write.
 * @param[in] iovcnt Number of @p iov items.
 * @return Number of bytes written, -1 on error.
 */
ssize_t embp_net_writev(embp_net_sock sock, const struct iovec *iov, int iovcnt);

/**
 * @brief Control a socket device.
 *
 * @param[in] sock Backend socket.
 * @param[in] cmd Command to perform.
 * @param[in,out] argp Command argument.
 * @return 0 on success, -1 on error.
 */
int embp_net_ioctl(embp_net_sock sock, long cmd, void *argp);

/**
 * @brief Manipulate socket properties.
 *
 * @param[in] sock Backend socket.
 * @param[in] cmd Command to perform.
 * @param[in] val Command argument.
 * @return Command-specific value, -1 on error.
 */
int embp_net_fcntl(embp_net_sock sock, int cmd, int val);

/**
 * @brief Poll socket FDs.
 *
 * Both the FDs on input and on output are generic FDP FDs, the socket
 * translation is performed internally.
 *
 * @param[in,out] fds FDs to poll.
 * @param[in] nfds Number of @p fds items.
 * @param[in] timeout Timeout in ms, 0 for none, -1 for infinite.
 * @return Number of FDs with an event, -1 on error.
 */
int embp_net_poll(struct pollfd *fds, nfds_t nfds, int timeout);

#endif /* EMB_POSIX_NET_H_ */
