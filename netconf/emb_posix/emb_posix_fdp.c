/**
 * @file emb_posix_fdp.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief embedded POSIX library, FD table management
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
#include <stdatomic.h>
#include <string.h>

#include "emb_posix.h"

struct fdp_s fdp[MAX_FDP_COUNT];

static atomic_uint_fast8_t fdp_next;    /**< next FD to use */
static atomic_uint_fast8_t fdp_count;   /**< number of opened FDs */

char *
embp_fd_shm_get_addr(int fd)
{
    char *addr = NULL;

    switch (FDP_TYPE(fd)) {
    case FDP_SHM:
        addr = fdp[fd].shm.seg->addr;
        break;
    case FDP_NET:
    case FDP_FS:
    case FDP_FIFO:
    case FDP_NONE:
        break;
    }

    return addr;
}

void
fdp_free(int fd)
{
    /* free FD */
    fdp[fd].type = FDP_NONE;

    /* decrease used FD count */
    atomic_fetch_sub_explicit(&fdp_count, 1, memory_order_relaxed);
}

int
fdp_new(void)
{
    int fd;

    /* check MAX_FDP_COUNT */
    if (atomic_load_explicit(&fdp_count, memory_order_relaxed) == MAX_FDP_COUNT) {
        errno = ENFILE;
        return -1;
    }

    /* find an empty FD */
    do {
        fd = atomic_fetch_add_explicit(&fdp_next, 1, memory_order_relaxed);

        if (fd == MAX_FDP_COUNT - 1) {
            /* reset fdp_next */
            atomic_store_explicit(&fdp_next, 0, memory_order_relaxed);
        }
    } while (fdp[fd].type);

    /* increase used FD count */
    atomic_fetch_add_explicit(&fdp_count, 1, memory_order_relaxed);

    return fd;
}

int
fdp_new_net(embp_net_sock sock)
{
    int fd;

    fd = fdp_new();
    if (fd == -1) {
        return -1;
    }

    /* reserve new FD */
    fdp[fd].type = FDP_NET;
    fdp[fd].net.sock = sock;
    return fd;
}

int
fdp_new_fs(embp_fs_file **file)
{
    int fd;

    fd = fdp_new();
    if (fd == -1) {
        return -1;
    }

    /* reserve new FD and return the file */
    fdp[fd].type = FDP_FS;
    fdp[fd].fs.lock = 0;
    *file = &fdp[fd].fs.file;
    return fd;
}

int
fdp_sock2fdp(embp_net_sock sock)
{
    int fd;

    for (fd = 0; fd < MAX_FDP_COUNT; ++fd) {
        if ((fdp[fd].type == FDP_NET) && (fdp[fd].net.sock == sock)) {
            return fd;
        }
    }

    return -1;
}

int
fdp_file_collision(const char *path)
{
    uint32_t tmp_prefix = 0;

    if (!strncmp(path, "/dev/shm/", 9)) {
        tmp_prefix = 9;
    } else if (!strncmp(path, "/tmp/", 5)) {
        tmp_prefix = 5;
    }

    /* SHM segment */
    if (tmp_prefix && fdp_find_shm(path + tmp_prefix)) {
        return -1;
    }

    /* FS file */
    if (embp_fs_exists(path)) {
        errno = EEXIST;
        return -1;
    }

    /* named FIFO */
    if (fdp_find_fifo(path)) {
        errno = EEXIST;
        return -1;
    }

    return 0;
}
