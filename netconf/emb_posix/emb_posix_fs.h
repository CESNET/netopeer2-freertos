/**
 * @file emb_posix_fs.h
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief embedded POSIX library, file system backend interface
 *
 * Internal contract implemented by exactly one backend module, so that the
 * storage dependency can be replaced without touching the POSIX layer. The
 * build system picks the module via EMB_POSIX_FS_SOURCE and its
 * emb_posix_fs_backend.h via EMB_POSIX_FS_INCLUDE_DIR, which also allows a
 * backend to live outside of this project.
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

#ifndef EMB_POSIX_FS_H_
#define EMB_POSIX_FS_H_

#include <sys/types.h>

/* embp_fs_file, embp_fs_dir, embp_fs_info, EMBP_FS_NAME_MAX, EMBP_FS_TYPE_REG */
#include "emb_posix_fs_backend.h"

/**
 * @brief Open a backend file.
 *
 * @param[in,out] file Backend file handle to fill, must stay at the same address until closed.
 * @param[in] path Path of the file to open.
 * @param[in] flags POSIX open() flags.
 * @return 0 on success;
 * @return -1 on error, errno set.
 */
int embp_fs_open(embp_fs_file *file, const char *path, int flags);

/**
 * @brief Close a backend file.
 *
 * @param[in] file Backend file handle.
 * @return 0 on success;
 * @return -1 on error, errno set.
 */
int embp_fs_close(embp_fs_file *file);

/**
 * @brief Read from a backend file.
 *
 * @param[in] file Backend file handle.
 * @param[out] buf Buffer to fill.
 * @param[in] count Number of bytes to read.
 * @return Number of bytes read;
 * @return -1 on error, errno set.
 */
ssize_t embp_fs_read(embp_fs_file *file, void *buf, size_t count);

/**
 * @brief Write into a backend file.
 *
 * @param[in] file Backend file handle.
 * @param[in] buf Data to write.
 * @param[in] count Number of bytes to write.
 * @return Number of bytes written;
 * @return -1 on error, errno set.
 */
ssize_t embp_fs_write(embp_fs_file *file, const void *buf, size_t count);

/**
 * @brief Flush a backend file onto the storage.
 *
 * @param[in] file Backend file handle.
 * @return 0 on success;
 * @return -1 on error, errno set.
 */
int embp_fs_sync(embp_fs_file *file);

/**
 * @brief Change the current offset of a backend file.
 *
 * @param[in] file Backend file handle.
 * @param[in] offset Offset to apply.
 * @param[in] whence SEEK_SET, SEEK_CUR, or SEEK_END.
 * @return New offset;
 * @return -1 on error, errno set.
 */
off_t embp_fs_seek(embp_fs_file *file, off_t offset, int whence);

/**
 * @brief Truncate a backend file.
 *
 * @param[in] file Backend file handle.
 * @param[in] length New file length.
 * @return 0 on success;
 * @return -1 on error, errno set.
 */
int embp_fs_truncate(embp_fs_file *file, off_t length);

/**
 * @brief Get the size of a backend file.
 *
 * @param[in] file Backend file handle.
 * @return File size;
 * @return -1 on error, errno set.
 */
off_t embp_fs_size(embp_fs_file *file);

/**
 * @brief Check whether a backend file exists.
 *
 * @param[in] path Path of the file.
 * @return 1 if it exists, 0 otherwise.
 */
int embp_fs_exists(const char *path);

/**
 * @brief Remove a backend file.
 *
 * @param[in] path Path of the file.
 * @return 0 on success;
 * @return -1 on error, errno set.
 */
int embp_fs_remove(const char *path);

/**
 * @brief Rename a backend file.
 *
 * @param[in] oldpath Current path of the file.
 * @param[in] newpath New path of the file.
 * @return 0 on success;
 * @return -1 on error, errno set.
 */
int embp_fs_rename(const char *oldpath, const char *newpath);

/**
 * @brief Create a backend directory.
 *
 * @param[in] path Path of the directory.
 * @return 0 on success;
 * @return -1 on error, errno set.
 */
int embp_fs_mkdir(const char *path);

/**
 * @brief Get information about a backend file or directory.
 *
 * @param[in] path Path to examine.
 * @param[out] info Filled information.
 * @return 0 on success;
 * @return -1 on error, errno set.
 */
int embp_fs_stat(const char *path, embp_fs_info *info);

/**
 * @brief Open a backend directory.
 *
 * @param[in,out] dir Backend directory handle to fill.
 * @param[in] name Path of the directory.
 * @return 0 on success;
 * @return -1 on error, errno set.
 */
int embp_fs_opendir(embp_fs_dir *dir, const char *name);

/**
 * @brief Close a backend directory.
 *
 * @param[in] dir Backend directory handle.
 * @return 0 on success;
 * @return -1 on error, errno set.
 */
int embp_fs_closedir(embp_fs_dir *dir);

/**
 * @brief Read the next entry of a backend directory.
 *
 * @param[in] dir Backend directory handle.
 * @param[out] info Filled entry information.
 * @return 1 if an entry was read;
 * @return 0 on the end of the directory;
 * @return -1 on error, errno set.
 */
int embp_fs_readdir(embp_fs_dir *dir, embp_fs_info *info);

/**
 * @brief Rewind a backend directory to its first entry.
 *
 * @param[in] dir Backend directory handle.
 */
void embp_fs_rewinddir(embp_fs_dir *dir);

/**
 * @brief Move a backend directory to a previously told location.
 *
 * @param[in] dir Backend directory handle.
 * @param[in] loc Location returned by ::embp_fs_telldir().
 */
void embp_fs_seekdir(embp_fs_dir *dir, long int loc);

/**
 * @brief Get the current location of a backend directory.
 *
 * @param[in] dir Backend directory handle.
 * @return Current location;
 * @return -1 on error, errno set.
 */
long int embp_fs_telldir(embp_fs_dir *dir);

#endif /* EMB_POSIX_FS_H_ */
