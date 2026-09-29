/**
 * @file emb_posix_net_backend.h
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief embedded POSIX library, lwIP backend types
 *
 * Selected by EMB_POSIX_NET_INCLUDE_DIR putting this directory on the include path.
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

#ifndef EMB_POSIX_NET_BACKEND_H_
#define EMB_POSIX_NET_BACKEND_H_

/** @brief Opaque backend socket handle, stored in the FD table. */
typedef int embp_net_sock;

#endif /* EMB_POSIX_NET_BACKEND_H_ */
