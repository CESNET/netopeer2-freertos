/**
 * @file emb_posix.h
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief embedded POSIX library header
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

/**
 * @brief Syslog logging callback.
 *
 * @param[in] identity Log source identity.
 * @param[in] priority Syslog message priority.
 * @param[in] msg Message to log.
 */
typedef void (*embp_syslog_cb)(const char *identity, int priority, const char *msg);

/**
 * @brief Set a callback to be called on a syslog message.
 *
 * @param[in] log_cb Callback to use for syslog logging.
 */
void embp_syslog_set_cb(embp_syslog_cb log_cb);

/**
 * @brief Get current address of a SHM segment data (and so all of its mappings).
 *
 * @param[in] fd SHM FD.
 * @return SHM segment data.
 * @return NULL on invalid FD.
 */
char *embp_fd_shm_get_addr(int fd);
