/**
 * @file emb_posix_fs_backend.h
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief embedded POSIX library, littleFS backend types and API
 *
 * Selected by EMB_POSIX_FS_INCLUDE_DIR putting this directory on the include path.
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

#ifndef EMB_POSIX_FS_BACKEND_H_
#define EMB_POSIX_FS_BACKEND_H_

#include "lfs.h"

/** @brief Opaque backend file handle, embedded in the FD table. */
typedef lfs_file_t embp_fs_file;

/** @brief Opaque backend directory handle, embedded in DIR. */
typedef lfs_dir_t embp_fs_dir;

/** @brief Backend file/directory information. */
typedef struct lfs_info embp_fs_info;

#define EMBP_FS_NAME_MAX LFS_NAME_MAX   /**< maximum length of a file name */
#define EMBP_FS_TYPE_REG LFS_TYPE_REG   /**< regular file type of ::embp_fs_info */

/**
 * @brief Get the LFS configuration to use.
 *
 * @return LFS configuration.
 */
struct lfs_config *embp_get_lfs_cfg(void);

/**
 * @brief Get the LFS instance.
 *
 * @return LFS instance.
 */
lfs_t *embp_get_lfs(void);

#endif /* EMB_POSIX_FS_BACKEND_H_ */
