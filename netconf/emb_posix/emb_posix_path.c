/**
 * @file emb_posix_path.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief embedded POSIX library, path based operations
 *
 * Resolves a path to the FDP type owning it (virtual SHM/tmp file, named
 * pipe, or a file system file) and dispatches the call accordingly.
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

#include "emb_posix_internal.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char cwd[256] = "/";     /**< current working directory */

void
embp_cwd_set_file(const char *path)
{
    char *ptr;

    strncpy(cwd, path, 255);
    ptr = strrchr(cwd, '/');
    if (ptr != cwd) {
        *ptr = '\0';
    }
}

void
embp_cwd_set_dir(const char *path)
{
    strncpy(cwd, path, 255);
    if (cwd[strlen(cwd) - 1] == '/') {
        cwd[strlen(cwd) - 1] = '\0';
    }
}

/* fcntl.h
 */

int
open(const char *pathname, int flags, ...)
{
    struct fdp_fifo *ff;
    embp_fs_file *file;
    int fd, tmp_len = 0;

    if (!strncmp(pathname, "/dev/shm/", 9)) {
        tmp_len = 9;
    } else if (!strncmp(pathname, "/tmp/", 5)) {
        tmp_len = 5;
    }

    if (tmp_len) {
        /* virtual SHM/temporary file */
        fd = fdp_new_shm(pathname + tmp_len, flags);
        if ((fd == -1) && (errno == ENOENT)) {
            /* may be an existing FIFO file */
            errno = 0;
        } else {
            return fd;
        }
    }

    /* try to open an existing fifo */
    ff = fdp_find_fifo(pathname);
    if (ff) {
        if (flags & ~(O_RDONLY | O_WRONLY | O_RDWR | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC)) {
            /* O_NONBLOCK supported, rest ignored and any other flags not supported */
            errno = ENOSYS;
            return -1;
        }

        fd = fdp_new();
        if (fd == -1) {
            return -1;
        }

        /* fill fifo FD */
        fdp[fd].type = FDP_FIFO;
        fdp[fd].fifo.file = ff;
        fdp[fd].fifo.flags = flags & O_NONBLOCK;
        atomic_fetch_add_explicit(&ff->refcount, 1, memory_order_relaxed);
        return fd;
    }

    if (tmp_len) {
        /* do not try to open tmp files on FS */
        errno = ENOENT;
        return -1;
    }

    /* mode ignored, FS permissions not supported */

    /* get new FD, the closed file must be exactly the same as the opened file (same address, not a copy) */
    fd = fdp_new_fs(&file);
    if (fd == -1) {
        return -1;
    }

    /* open the file */
    if (embp_fs_open(file, pathname, flags)) {
        fdp_free(fd);
        return -1;
    }

    /* store this file directory as CWD */
    embp_cwd_set_file(pathname);

    return fd;
}

int
creat(const char *pathname, mode_t mode)
{
    return open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

/* unistd.h
 */

char *
getcwd(char buf[], size_t size)
{
    if (size < strlen(cwd) + 1) {
        errno = ERANGE;
        return NULL;
    }

    strcpy(buf, cwd);
    return buf;
}

char *
get_current_dir_name(void)
{
    return strdup(cwd);
}

ssize_t
readlink(const char *pathname, char *buf, size_t bufsiz)
{
    errno = EOPNOTSUPP;
    return -1;
}

int
chmod(const char *path, mode_t mode)
{
    return 0;
}

int
chown(const char *path, uid_t owner, gid_t group)
{
    return 0;
}

int
access(const char *path, int mode)
{
    int fd;
    DIR *dir;

    /* file */
    fd = open(path, O_RDONLY);
    if (fd > -1) {
        close(fd);
        return 0;
    }

    /* dir */
    dir = opendir(path);
    if (dir) {
        closedir(dir);
        return 0;
    }

    errno = ENOENT;
    return -1;
}

int
eaccess(const char *path, int mode)
{
    return access(path, mode);
}

int
unlink(const char *pathname)
{
    struct fdp_shm *seg;
    struct fdp_fifo *ff;

    if (!strncmp(pathname, "/dev/shm/", 9) || !strncmp(pathname, "/tmp/", 5)) {
        if (!strncmp(pathname, "/dev/shm/", 9)) {
            pathname += 9;
        } else {
            pathname += 5;
        }

        /* find the segment */
        seg = fdp_find_shm(pathname);
        if (!seg) {
            errno = ENOENT;
            return -1;
        }

        /* free it */
        fdp_free_shm(seg);
        return 0;
    }

    /* try to find a fifo */
    ff = fdp_find_fifo(pathname);
    if (ff) {
        fdp_free_fifo(ff);
        return 0;
    }

    /* remove a FS file */
    return embp_fs_remove(pathname);
}

/* sys/stat.h
 */

int
mkdir(const char *path, mode_t mode)
{
    if (!strcmp(path, "/tmp") || !strncmp(path, "/tmp/", 5)) {
        /* virtual directories */
        return 0;
    }

    return embp_fs_mkdir(path);
}

int
stat(const char *path, struct stat *sbuf)
{
    embp_fs_info info;
    struct fdp_shm *seg;

    if (!strncmp(path, "/dev/shm/", 9) || !strncmp(path, "/tmp/", 5)) {
        if (!strncmp(path, "/dev/shm/", 9)) {
            path += 9;
        } else {
            path += 5;
        }

        seg = fdp_find_shm(path);
        if (!seg) {
            errno = ENOENT;
            return -1;
        }

        info.type = EMBP_FS_TYPE_REG;
        info.size = seg->size;
    } else if (embp_fs_stat(path, &info)) {
        return -1;
    }

    memset(sbuf, 0, sizeof *sbuf);
    if (info.type == EMBP_FS_TYPE_REG) {
        sbuf->st_mode = S_IFREG | DEFFILEMODE; // 00666
    } else {
        sbuf->st_mode = S_IFDIR | ACCESSPERMS; // 00777
    }
    sbuf->st_size = info.size;
    return 0;
}

mode_t
umask(mode_t mask)
{
    return S_IWGRP | S_IWOTH;
}

/* stdio.h
 */

int
remove(const char *pathname)
{
    return unlink(pathname);
}

int
rename(const char *oldpath, const char *newpath)
{
    if (!strncmp(oldpath, "/dev/shm/", 9) || !strncmp(oldpath, "/tmp/", 5)) {
        if (!strncmp(oldpath, "/dev/shm/", 9)) {
            if (strncmp(newpath, "/dev/shm/", 9)) {
                errno = EINVAL;
                return -1;
            }

            oldpath += 9;
            newpath += 9;
        } else {
            if (strncmp(oldpath, "/tmp/", 5)) {
                errno = EINVAL;
                return -1;
            }

            oldpath += 5;
            newpath += 5;
        }

        /* only a virtual SHM/temporary file */
        return fdp_rename_shm(oldpath, newpath);
    }

    /* rename */
    return embp_fs_rename(oldpath, newpath);
}

/* stdlib.h
 */

char *
realpath(const char *path, char *resolved_path)
{
    if (resolved_path) {
        snprintf(resolved_path, PATH_MAX, "%s", path);
        return resolved_path;
    } else {
        return strdup(path);
    }
}

int
mkstemp(char *template)
{
    char *ptr;
    int i, r, n, count, fd;

    if (!template || (strlen(template) < 6) || strcmp(template + (strlen(template) - 6), "XXXXXX")) {
        errno = EINVAL;
        return -1;
    }

retry:
    /* generate the suffix */
    ptr = template + (strlen(template) - 6);
    for (i = 0; i < 6; ++i) {
        r = rand();
        n = rand();
        switch (r % 3) {
        case 0:
            /* number */
            count = (('9' - '0') + 1);
            *ptr = '0' + (n % count);
            break;
        case 1:
            /* uppercase */
            count = (('Z' - 'A') + 1);
            *ptr = 'A' + (n % count);
            break;
        case 2:
            /* lowercase */
            count = (('z' - 'a') + 1);
            *ptr = 'a' + (n % count);
            break;
        }

        ++ptr;
    }

    /* open the file in the root */
    fd = open(template, O_RDWR | O_CREAT | O_EXCL);
    if ((fd < 0) && (errno == EEXIST)) {
        /* exists already, try again */
        goto retry;
    }

    return fd;
}
