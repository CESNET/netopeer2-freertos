/**
 * @file emb_posix_fs_lfs.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief embedded POSIX library, littleFS file system backend
 *
 * The only module calling the littleFS API. Default backend of the library.
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

#define _GNU_SOURCE

#include "emb_posix_fs.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

static lfs_t embp_lfs;                  /**< global mounted littleFS, it is not thread-safe */
static struct lfs_config embp_lfs_cfg;  /**< global mounted littleFS configuration */
static pthread_mutex_t embp_lfs_lock = PTHREAD_MUTEX_INITIALIZER;

struct lfs_config *
embp_get_lfs_cfg(void)
{
    return &embp_lfs_cfg;
}

lfs_t *
embp_get_lfs(void)
{
    return &embp_lfs;
}

/**
 * @brief Transform a littleFS return code into a POSIX one.
 *
 * @param[in] r littleFS return code.
 * @return @p r if not negative;
 * @return -1 otherwise, errno set.
 */
static int
fs_rc(int r)
{
    if (r < 0) {
        errno = -r;
        return -1;
    }

    return r;
}

/**
 * @brief Transform POSIX open() flags into littleFS ones.
 *
 * @param[in] flags POSIX open() flags.
 * @return littleFS open flags.
 */
static int
fs_open_flags(int flags)
{
    int lfs_flags = 0;

    /* ignore flags not supported by fs */
    if (flags & O_RDONLY) {
        lfs_flags |= LFS_O_RDONLY;
    }
    if (flags & O_WRONLY) {
        lfs_flags |= LFS_O_WRONLY;
    }
    if (flags & O_APPEND) {
        lfs_flags |= LFS_O_APPEND;
    }
    if (flags & O_CREAT) {
        lfs_flags |= LFS_O_CREAT;
    }
    if (flags & O_TRUNC) {
        lfs_flags |= LFS_O_TRUNC;
    }
    if (flags & O_EXCL) {
        lfs_flags |= LFS_O_EXCL;
    }

    return lfs_flags;
}

int
embp_fs_open(embp_fs_file *file, const char *path, int flags)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_file_open(&embp_lfs, file, path, fs_open_flags(flags));
    pthread_mutex_unlock(&embp_lfs_lock);

    return fs_rc(r);
}

int
embp_fs_close(embp_fs_file *file)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_file_close(&embp_lfs, file);
    pthread_mutex_unlock(&embp_lfs_lock);

    return fs_rc(r);
}

ssize_t
embp_fs_read(embp_fs_file *file, void *buf, size_t count)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_file_read(&embp_lfs, file, buf, count);
    pthread_mutex_unlock(&embp_lfs_lock);

    return fs_rc(r);
}

ssize_t
embp_fs_write(embp_fs_file *file, const void *buf, size_t count)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_file_write(&embp_lfs, file, buf, count);
    pthread_mutex_unlock(&embp_lfs_lock);

    return fs_rc(r);
}

int
embp_fs_sync(embp_fs_file *file)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_file_sync(&embp_lfs, file);
    pthread_mutex_unlock(&embp_lfs_lock);

    return fs_rc(r);
}

off_t
embp_fs_seek(embp_fs_file *file, off_t offset, int whence)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_file_seek(&embp_lfs, file, offset, whence);
    pthread_mutex_unlock(&embp_lfs_lock);

    return fs_rc(r);
}

int
embp_fs_truncate(embp_fs_file *file, off_t length)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_file_truncate(&embp_lfs, file, length);
    pthread_mutex_unlock(&embp_lfs_lock);

    return fs_rc(r);
}

off_t
embp_fs_size(embp_fs_file *file)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_file_size(&embp_lfs, file);
    pthread_mutex_unlock(&embp_lfs_lock);

    return fs_rc(r);
}

int
embp_fs_exists(const char *path)
{
    lfs_file_t file;
    int exists = 0;

    pthread_mutex_lock(&embp_lfs_lock);
    if (lfs_file_open(&embp_lfs, &file, path, LFS_O_RDONLY) == 0) {
        lfs_file_close(&embp_lfs, &file);
        exists = 1;
    }
    pthread_mutex_unlock(&embp_lfs_lock);

    return exists;
}

int
embp_fs_remove(const char *path)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_remove(&embp_lfs, path);
    pthread_mutex_unlock(&embp_lfs_lock);

    return fs_rc(r);
}

int
embp_fs_rename(const char *oldpath, const char *newpath)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_rename(&embp_lfs, oldpath, newpath);
    pthread_mutex_unlock(&embp_lfs_lock);

    return fs_rc(r);
}

int
embp_fs_mkdir(const char *path)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_mkdir(&embp_lfs, path);
    pthread_mutex_unlock(&embp_lfs_lock);

    return fs_rc(r);
}

int
embp_fs_stat(const char *path, embp_fs_info *info)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_stat(&embp_lfs, path, info);
    pthread_mutex_unlock(&embp_lfs_lock);

    return fs_rc(r);
}

int
embp_fs_opendir(embp_fs_dir *dir, const char *name)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_dir_open(&embp_lfs, dir, name);
    pthread_mutex_unlock(&embp_lfs_lock);

    return fs_rc(r);
}

int
embp_fs_closedir(embp_fs_dir *dir)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_dir_close(&embp_lfs, dir);
    pthread_mutex_unlock(&embp_lfs_lock);

    return fs_rc(r);
}

int
embp_fs_readdir(embp_fs_dir *dir, embp_fs_info *info)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_dir_read(&embp_lfs, dir, info);
    pthread_mutex_unlock(&embp_lfs_lock);

    if (r < 0) {
        errno = -r;
        return -1;
    }

    /* 0 on the end of the directory */
    return r ? 1 : 0;
}

void
embp_fs_rewinddir(embp_fs_dir *dir)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_dir_rewind(&embp_lfs, dir);
    pthread_mutex_unlock(&embp_lfs_lock);

    fs_rc(r);
}

void
embp_fs_seekdir(embp_fs_dir *dir, long int loc)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_dir_seek(&embp_lfs, dir, loc);
    pthread_mutex_unlock(&embp_lfs_lock);

    fs_rc(r);
}

long int
embp_fs_telldir(embp_fs_dir *dir)
{
    int r;

    pthread_mutex_lock(&embp_lfs_lock);
    r = lfs_dir_tell(&embp_lfs, dir);
    pthread_mutex_unlock(&embp_lfs_lock);

    return fs_rc(r);
}
