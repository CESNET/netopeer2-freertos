/**
 * @file emb_posix_fd.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief embedded POSIX library, generic FD operations
 *
 * Dispatches the FD-based POSIX calls to the backend matching the FD type.
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

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#include "emb_posix_net.h"

/* unistd.h
 */

int
close(int fd)
{
    int rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = embp_net_close(fdp[fd].net.sock);
        break;
    case FDP_FS:
        rc = embp_fs_close(&fdp[fd].fs.file);
        break;
    case FDP_SHM:
        fdp_free_shm(fdp[fd].shm.seg);
        break;
    case FDP_FIFO:
        fdp_free_fifo(fdp[fd].fifo.file);
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    if (!rc) {
        fdp_free(fd);
    }
    return rc;
}

ssize_t
read(int fd, void *buf, size_t count)
{
    ssize_t rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = embp_net_read(fdp[fd].net.sock, buf, count);
        break;
    case FDP_FS:
        rc = embp_fs_read(&fdp[fd].fs.file, buf, count);
        break;
    case FDP_SHM:
        rc = embp_shm_read(fd, buf, count);
        break;
    case FDP_FIFO:
        rc = embp_fifo_read(fd, buf, count);
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

ssize_t
write(int fd, const void *buf, size_t count)
{
    ssize_t rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = embp_net_write(fdp[fd].net.sock, buf, count);
        break;
    case FDP_FS:
        rc = embp_fs_write(&fdp[fd].fs.file, buf, count);
        break;
    case FDP_SHM:
        rc = embp_shm_write(fd, buf, count);
        break;
    case FDP_FIFO:
        rc = embp_fifo_write(fd, buf, count);
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

int
fsync(int fd)
{
    int rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_FS:
        rc = embp_fs_sync(&fdp[fd].fs.file);
        break;
    case FDP_NET:
    case FDP_SHM:
    case FDP_FIFO:
        /* no buffering so nothing to do */
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

off_t
lseek(int fd, off_t offset, int whence)
{
    off_t rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_FS:
        rc = embp_fs_seek(&fdp[fd].fs.file, offset, whence);
        break;
    case FDP_NET:
    case FDP_FIFO:
        errno = ESPIPE;
        rc = -1;
        break;
    case FDP_SHM:
        errno = ENOSYS;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

int
ftruncate(int fd, off_t length)
{
    int rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_FS:
        rc = embp_fs_truncate(&fdp[fd].fs.file, length);
        break;
    case FDP_SHM:
        rc = fdp_resize_shm(fdp[fd].shm.seg, length);
        break;
    case FDP_NET:
    case FDP_FIFO:
        errno = EINVAL;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

int
dup2(int oldfd, int newfd)
{
    errno = ENOSYS;
    return -1;
}

/* sys/uio.h
 */

ssize_t
readv(int fd, const struct iovec *iov, int iovcnt)
{
    ssize_t rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = embp_net_readv(fdp[fd].net.sock, iov, iovcnt);
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOSYS;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

ssize_t
writev(int fd, const struct iovec *iov, int iovcnt)
{
    ssize_t rc = 0, r;
    int i;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = embp_net_writev(fdp[fd].net.sock, iov, iovcnt);
        break;
    case FDP_SHM:
    case FDP_FIFO:
    case FDP_FS:
        for (i = 0; i < iovcnt; ++i) {
            r = write(fd, iov[i].iov_base, iov[i].iov_len);
            if (r == -1) {
                /* error */
                rc = -1;
                break;
            }

            rc += r;
        }
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

/* poll.h
 */

int
poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    int rc = 0;
    nfds_t i;
    enum fdp_type type = FDP_NONE;
    short events;

    /* check all FDs are of the same kind */
    for (i = 0; i < nfds; ++i) {
        if (fds[i].fd < 0) {
            /* ignored */
            continue;
        }

        /* check all FDs are of the same kind */
        if (type == FDP_NONE) {
            type = FDP_TYPE(fds[i].fd);
            events = fds[i].events;
        } else if (type != FDP_TYPE(fds[i].fd)) {
            errno = ENOSYS;
            return -1;
        }

        /* zero revents */
        fds[i].revents = 0;

        switch (FDP_TYPE(fds[i].fd)) {
        case FDP_NET:
            /* because of the backend poll() problems, we can support only the same events on all the sockets */
            if (events != fds[i].events) {
                errno = ENOSYS;
                return -1;
            }
            break;
        case FDP_FIFO:
        case FDP_NONE:
            /* nothing to check */
            break;
        case FDP_FS:
        case FDP_SHM:
            errno = ENOSYS;
            return -1;
        }
    }

    /* poll */
    switch (type) {
    case FDP_NET:
        rc = embp_net_poll(fds, nfds, timeout);
        break;
    case FDP_FIFO:
        rc = embp_fifo_poll(fds, nfds, timeout);
        break;
    case FDP_NONE:
        /* just set POLLNVAL for closed FDs */
        for (i = 0; i < nfds; ++i) {
            if (fds[i].fd < 0) {
                /* ignored */
                continue;
            }

            fds[i].revents = POLLNVAL;
            ++rc;
        };
        break;
    case FDP_FS:
    case FDP_SHM:
        rc = -1;
        break;
    }

    return rc;
}

/* sys/ioctl.h
 */

int
ioctl(int fd, long cmd, void *argp)
{
    int rc = 0;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        rc = embp_net_ioctl(fdp[fd].net.sock, cmd, argp);
        break;
    case FDP_FS:
    case FDP_SHM:
    case FDP_FIFO:
        errno = ENOTTY;
        rc = -1;
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

/* fcntl.h
 */

int
fcntl(int fd, int cmd, ...)
{
    va_list va;
    int rc = 0, val;
    struct flock *fl;
    enum fdp_fs_lock *lock_p = NULL;

    switch (FDP_TYPE(fd)) {
    case FDP_NET:
        va_start(va, cmd);
        val = va_arg(va, int);
        va_end(va);

        rc = embp_net_fcntl(fdp[fd].net.sock, cmd, val);
        break;
    case FDP_SHM:
        lock_p = &fdp[fd].shm.lock;
    /* fallthrough */
    case FDP_FS:
        if (!lock_p) {
            lock_p = &fdp[fd].fs.lock;
        }

        switch (cmd) {
        case F_SETLK:
        case F_SETLKW:
            va_start(va, cmd);
            fl = va_arg(va, struct flock *);
            va_end(va);

            if (fl->l_type == F_WRLCK) {
                /* write lock */
                if (*lock_p != FDP_FS_NONE) {
                    errno = EAGAIN;
                    rc = -1;
                } else {
                    *lock_p = FDP_FS_WLOCK;
                }
            } else if (fl->l_type == F_UNLCK) {
                /* unlock */
                if (*lock_p == FDP_FS_NONE) {
                    errno = EINVAL;
                    rc = -1;
                } else {
                    *lock_p = FDP_FS_NONE;
                }
            } else {
                errno = EINVAL;
                rc = -1;
            }
            break;
        case F_GETLK:
            va_start(va, cmd);
            fl = va_arg(va, struct flock *);
            va_end(va);

            /* return current lock */
            memset(fl, 0, sizeof *fl);
            switch (*lock_p) {
            case FDP_FS_NONE:
                fl->l_type = F_UNLCK;
                break;
            case FDP_FS_WLOCK:
                fl->l_type = F_WRLCK;
                break;
            }
            break;
        default:
            errno = ENOSYS;
            rc = -1;
            break;
        }
        break;
    case FDP_FIFO:
        switch (cmd) {
        case F_GETFL:
            rc = fdp[fd].fifo.flags;
            break;
        case F_SETFL:
            va_start(va, cmd);
            val = va_arg(va, int);
            va_end(va);

            if (val && (val != O_NONBLOCK)) {
                errno = ENOSYS;
                rc = -1;
                break;
            }

            fdp[fd].fifo.flags = val;
            rc = 0;
            break;
        default:
            errno = ENOSYS;
            rc = -1;
            break;
        }
        break;
    case FDP_NONE:
        errno = EBADF;
        rc = -1;
        break;
    }

    return rc;
}

/* sys/stat.h
 */

int
fstat(int fd, struct stat *sbuf)
{
    memset(sbuf, 0, sizeof *sbuf);

    switch (FDP_TYPE(fd)) {
    case FDP_FS:
        sbuf->st_mode = S_IFREG | DEFFILEMODE; // 00666
        sbuf->st_size = embp_fs_size(&fdp[fd].fs.file);
        break;
    case FDP_SHM:
        sbuf->st_mode = S_IFREG | DEFFILEMODE; // 00666
        sbuf->st_size = fdp[fd].shm.seg->size;
        break;
    case FDP_FIFO:
        sbuf->st_mode = S_IFIFO | DEFFILEMODE; // 00666
        break;
    case FDP_NET:
        sbuf->st_mode = S_IFSOCK;
        break;
    case FDP_NONE:
        errno = EBADF;
        return -1;
    }

    return 0;
}

int
fchmod(int fildes, mode_t mode)
{
    return 0;
}

int
fchown(int fildes, uid_t owner, gid_t group)
{
    return 0;
}
