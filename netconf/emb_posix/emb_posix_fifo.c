/**
 * @file emb_posix_fifo.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief embedded POSIX library, named and unnamed pipes
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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <FreeRTOS.h>
#include <stream_buffer.h>

/**
 * @brief Array of existing named and unnamed pipes.
 *
 * Is an array of pointers to enable calling realloc() on the array without
 * changing the pipes themselves because they are referenced in fdp.
 */
static struct fdp_fifo **fifo;
static uint32_t fifo_count;
static pthread_mutex_t fifo_lock = PTHREAD_MUTEX_INITIALIZER;

struct fdp_fifo *
fdp_find_fifo(const char *path)
{
    struct fdp_fifo *ff = NULL;
    uint32_t i;

    pthread_mutex_lock(&fifo_lock);

    for (i = 0; i < fifo_count; ++i) {
        if (!strcmp(fifo[i]->path, path)) {
            ff = fifo[i];
            break;
        }
    }

    pthread_mutex_unlock(&fifo_lock);

    return ff;
}

struct fdp_fifo *
fdp_new_fifo(const char *path)
{
    struct fdp_fifo *ff = NULL;
    void *mem;

    pthread_mutex_lock(&fifo_lock);

    /* create a new fifo */
    mem = realloc(fifo, (fifo_count + 1) * sizeof *fifo);
    if (!mem) {
        goto error_mem;
    }
    fifo = mem;

    ff = calloc(1, sizeof *ff);
    if (!ff) {
        goto error_mem;
    }

    /* fill it */
    if (path) {
        ff->path = strdup(path);
        if (!ff->path) {
            goto error_mem;
        }
    }
    ff->stream_buf_storage = malloc(FIFO_BUFFER_SIZE + 1);
    if (!ff->stream_buf_storage) {
        goto error_mem;
    }
    ff->stream_buf = xStreamBufferCreateStatic(FIFO_BUFFER_SIZE, 1, ff->stream_buf_storage, &ff->stream_buf_struct);
    if (!ff->stream_buf) {
        goto error_mem;
    }
    ff->refcount = 1;

    /* store */
    fifo[fifo_count] = ff;
    ++fifo_count;

    pthread_mutex_unlock(&fifo_lock);
    return ff;

error_mem:
    pthread_mutex_unlock(&fifo_lock);

    if (ff) {
        free(ff->path);
        free(ff->stream_buf_storage);
        free(ff);
    }

    errno = ENOMEM;
    return NULL;
}

void
fdp_free_fifo(struct fdp_fifo *ff)
{
    uint32_t i;

    if (!ff) {
        return;
    }

    if (atomic_fetch_sub_explicit(&ff->refcount, 1, memory_order_relaxed) > 1) {
        return;
    }

    pthread_mutex_lock(&fifo_lock);

    /* free the fifo file */
    free(ff->path);
    vStreamBufferDelete(ff->stream_buf);
    free(ff->stream_buf_storage);
    free(ff);

    /* replace by the last file, if not last */
    for (i = 0; i < fifo_count; ++i) {
        if (fifo[i] == ff) {
            break;
        }
    }
    assert(i < fifo_count);
    if (i < fifo_count - 1) {
        fifo[i] = fifo[fifo_count - 1];
    }
    --fifo_count;

    if (!fifo_count) {
        /* no fifo files left */
        free(fifo);
        fifo = NULL;
    }

    pthread_mutex_unlock(&fifo_lock);
}

ssize_t
embp_fifo_read(int fd, void *buf, size_t count)
{
    struct fdp_fifo *ff = fdp[fd].fifo.file;
    ssize_t rc;

    if (fdp[fd].fifo.flags & O_NONBLOCK) {
        rc = xStreamBufferReceive(ff->stream_buf, buf, count, 0);
        if (!rc) {
            /* no data available */
            errno = EAGAIN;
            return -1;
        }
        return rc;
    }

    /* keep reading until count bytes are read */
    rc = 0;
    do {
        rc += xStreamBufferReceive(ff->stream_buf, (char *)buf + rc, count - rc, portMAX_DELAY);
    } while (rc < count);

    return rc;
}

ssize_t
embp_fifo_write(int fd, const void *buf, size_t count)
{
    struct fdp_fifo *ff = fdp[fd].fifo.file;
    ssize_t rc;

    if (ff->refcount == 1) {
        /* opened only by this task */
        errno = EPIPE;
        return -1;
    }

    if (fdp[fd].fifo.flags & O_NONBLOCK) {
        rc = xStreamBufferSend(ff->stream_buf, buf, count, 0);
        if (!rc) {
            /* buffer full */
            errno = EAGAIN;
            return -1;
        }
        return rc;
    }

    /* write all the data */
    rc = 0;
    do {
        rc += xStreamBufferSend(ff->stream_buf, (char *)buf + rc, count - rc, portMAX_DELAY);
    } while (rc < count);

    return rc;
}

int
embp_fifo_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000};
    short revents_ignore;
    nfds_t i;
    int rc;

    while (1) {
        /* check all pipes */
        rc = 0;
        for (i = 0; i < nfds; ++i) {
            if (fds[i].fd < 0) {
                /* ignored */
                continue;
            }

            /* POLLOUT is always available so wait for other events on timeout */
            revents_ignore = ((fds[i].events & POLLIN) && timeout) ? POLLOUT : 0;

            if (fdp[fds[i].fd].fifo.file->refcount == 1) {
                /* other end hang up */
                fds[i].revents |= POLLHUP;
            } else if (fds[i].events & POLLOUT) {
                /* always available if opened */
                fds[i].revents |= POLLOUT;
            }
            if ((fds[i].events & POLLIN) && xStreamBufferBytesAvailable(fdp[fds[i].fd].fifo.file->stream_buf)) {
                /* data to read */
                fds[i].revents |= POLLIN;
            }

            if (fds[i].revents & ~revents_ignore) {
                ++rc;
            }
        }
        if (rc || !timeout) {
            /* an event or a timeout */
            break;
        }

        /* wait and retry */
        nanosleep(&ts, NULL);
        if (timeout > 0) {
            if (timeout >= 10) {
                timeout -= 10;
            } else {
                timeout = 0;
            }
        }
    }

    return rc;
}

/* unistd.h
 */

int
pipe(int pipefd[2])
{
    struct fdp_fifo *ff;

    /* create a new unnamed pipe */
    ff = fdp_new_fifo(NULL);
    if (!ff) {
        return -1;
    }

    pipefd[0] = fdp_new();
    pipefd[1] = fdp_new();
    if ((pipefd[0] == -1) || (pipefd[1] == -1)) {
        fdp_free_fifo(ff);
        return -1;
    }

    /* fill fifo FD #1 */
    fdp[pipefd[0]].type = FDP_FIFO;
    fdp[pipefd[0]].fifo.file = ff;

    /* fill fifo FD #2 */
    fdp[pipefd[1]].type = FDP_FIFO;
    fdp[pipefd[1]].fifo.file = ff;

    /* set refcount to 2 */
    atomic_fetch_add_explicit(&ff->refcount, 1, memory_order_relaxed);

    return 0;
}

int
pipe2(int pipefd[2], int flags)
{
    /* ignored flag */
    flags &= ~O_CLOEXEC;
    if (flags) {
        errno = ENOSYS;
        return -1;
    }

    return pipe(pipefd);
}

/* sys/stat.h
 */

int
mkfifo(const char *pathname, mode_t mode)
{
    /* check for existing files */
    if (fdp_file_collision(pathname)) {
        return -1;
    }

    /* create a new named fifo */
    if (!fdp_new_fifo(pathname)) {
        return -1;
    }

    return 0;
}
