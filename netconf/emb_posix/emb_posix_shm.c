/**
 * @file emb_posix_shm.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief embedded POSIX library, virtual SHM segments and memory mapping
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
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

/**
 * @brief Array of existing SHM files.
 *
 * Is an array of pointers to enable calling realloc() on the array without
 * changing the SHM segments themselves because they are referenced in fdp.
 */
static struct fdp_shm **shm;
static uint32_t shm_count;
static pthread_mutex_t shm_lock = PTHREAD_MUTEX_INITIALIZER;

/* static memory for the printed YANG context, defined in the linker script */
extern char __netconfContextStart[];
extern char __netconfContextEnd[];

struct fdp_shm *
fdp_find_shm(const char *file_name)
{
    struct fdp_shm *seg = NULL;
    uint32_t i;

    pthread_mutex_lock(&shm_lock);

    for (i = 0; i < shm_count; ++i) {
        if (!strcmp(shm[i]->file_name, file_name)) {
            seg = shm[i];
            break;
        }
    }

    pthread_mutex_unlock(&shm_lock);

    return seg;
}

struct fdp_shm *
fdp_next_shm(const char *dir_name, uint32_t *next_idx)
{
    struct fdp_shm *seg = NULL;
    uint32_t i, dir_len;

    dir_len = strlen(dir_name);

    pthread_mutex_lock(&shm_lock);

    for (i = *next_idx; i < shm_count; ++i) {
        if (!dir_len && !strchr(shm[i]->file_name, '/')) {
            /* tmp dir file */
            seg = shm[i];
            break;
        }

        if (dir_len && !strncmp(shm[i]->file_name, dir_name, dir_len) && (shm[i]->file_name[dir_len] == '/') &&
                !strchr(shm[i]->file_name + dir_len + 1, '/')) {
            /* nested tmp dir file */
            seg = shm[i];
            break;
        }
    }

    pthread_mutex_unlock(&shm_lock);

    *next_idx = i + 1;
    return seg;
}

int
fdp_resize_shm(struct fdp_shm *seg, uint32_t size)
{
    void *mem;

    if (seg->size == size) {
        /* nothing to do */
        return 0;
    }

    mem = realloc(seg->addr, size);
    if (!mem) {
        errno = ENOMEM;
        return -1;
    }

    seg->addr = mem;

    if (seg->size < size) {
        /* zero the new memory */
        memset(seg->addr + seg->size, 0, size - seg->size);
    }

    seg->size = size;
    return 0;
}

int
fdp_new_shm(const char *file_name, int flags)
{
    struct fdp_shm *seg;
    void *mem;
    int fd;

    /* try to find an existing SHM segment with the same name */
    seg = fdp_find_shm(file_name);
    if (seg && (flags & O_CREAT) && (flags & O_EXCL)) {
        errno = EEXIST;
        return -1;
    } else if (!seg && !(flags & O_CREAT)) {
        errno = ENOENT;
        return -1;
    }

    if (!seg) {
        pthread_mutex_lock(&shm_lock);

        /* create a new SHM segment */
        mem = realloc(shm, (shm_count + 1) * sizeof *shm);
        if (!mem) {
            errno = ENOMEM;
            pthread_mutex_unlock(&shm_lock);
            return -1;
        }
        shm = mem;

        seg = calloc(1, sizeof *seg);
        if (!seg) {
            errno = ENOMEM;
            pthread_mutex_unlock(&shm_lock);
            return -1;
        }

        shm[shm_count] = seg;
        ++shm_count;

        /* fill it */
        seg->file_name = strdup(file_name);
        if (!seg->file_name) {
            errno = ENOMEM;
            --shm_count;
            pthread_mutex_unlock(&shm_lock);
            return -1;
        }
        seg->refcount = 1;

        pthread_mutex_unlock(&shm_lock);
    }

    /* get a new FD */
    fd = fdp_new();
    if (fd == -1) {
        return -1;
    }

    /* reserve and fill new FD */
    fdp[fd].type = FDP_SHM;
    fdp[fd].shm.seg = seg;
    fdp[fd].shm.cur_pos = (flags & O_APPEND) ? seg->size : 0;
    atomic_fetch_add_explicit(&seg->refcount, 1, memory_order_relaxed);

    return fd;
}

void
fdp_free_shm(struct fdp_shm *seg)
{
    uint32_t i;

    /* decrease refcount */
    if (atomic_fetch_sub_explicit(&seg->refcount, 1, memory_order_relaxed) > 1) {
        return;
    }

    pthread_mutex_lock(&shm_lock);

    /* free the segment */
    free(seg->file_name);
    free(seg->addr);
    free(seg);

    /* replace by the last segment, if not last */
    for (i = 0; i < shm_count; ++i) {
        if (shm[i] == seg) {
            break;
        }
    }
    assert(i < shm_count);
    if (i < shm_count - 1) {
        shm[i] = shm[shm_count - 1];
    }
    --shm_count;

    if (!shm_count) {
        /* no SHM segments left */
        free(shm);
        shm = NULL;
    }

    pthread_mutex_unlock(&shm_lock);
}

int
fdp_rename_shm(const char *old_file_name, const char *new_file_name)
{
    struct fdp_shm *old_seg, *new_seg;

    /* find the old file */
    old_seg = fdp_find_shm(old_file_name);
    if (!old_seg) {
        errno = ENOENT;
        return -1;
    }

    /* try to find the new file */
    new_seg = fdp_find_shm(new_file_name);
    if (new_seg) {
        /* will be "overwritten", remove it */
        assert(atomic_load_explicit(&new_seg->refcount, memory_order_relaxed) == 1);
        fdp_free_shm(new_seg);
    }

    /* update it */
    free(old_seg->file_name);
    old_seg->file_name = strdup(new_file_name);
    if (!old_seg->file_name) {
        errno = ENOMEM;
        return -1;
    }

    return 0;
}

ssize_t
embp_shm_read(int fd, void *buf, size_t count)
{
    if (fdp[fd].shm.cur_pos > fdp[fd].shm.seg->size) {
        /* SHM could have been shrinked */
        fdp[fd].shm.cur_pos = fdp[fd].shm.seg->size;
    }
    if (fdp[fd].shm.cur_pos + count >= fdp[fd].shm.seg->size) {
        /* do not read past SHM size */
        count = fdp[fd].shm.seg->size - fdp[fd].shm.cur_pos;
    }

    memcpy(buf, fdp[fd].shm.seg->addr + fdp[fd].shm.cur_pos, count);
    fdp[fd].shm.cur_pos += count;
    return count;
}

ssize_t
embp_shm_write(int fd, const void *buf, size_t count)
{
    if (fdp[fd].shm.cur_pos + count > fdp[fd].shm.seg->size) {
        /* enlarge the SHM */
        if (fdp_resize_shm(fdp[fd].shm.seg, fdp[fd].shm.cur_pos + count)) {
            return -1;
        }
    }

    memcpy(fdp[fd].shm.seg->addr + fdp[fd].shm.cur_pos, buf, count);
    fdp[fd].shm.cur_pos += count;
    return count;
}

/* sys/mman.h
 */

void *
mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    struct fdp_shm *seg;

    if (addr && (addr == __netconfContextStart)) {
        if (((char *)addr) + len > __netconfContextEnd) {
            errno = ENOMEM;
            return MAP_FAILED;
        }

        /* always available, static memory chunk */
        return addr;
    }

    if (addr || offset || (flags & MAP_ANONYMOUS)) {
        errno = ENOSYS;
        return MAP_FAILED;
    }
    if ((fd >= MAX_FDP_COUNT) || (fd < 0)) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    if (fdp[fd].type != FDP_SHM) {
        errno = ENOSYS;
        return MAP_FAILED;
    }

    seg = fdp[fd].shm.seg;

    /* enlarge SHM if needed */
    if ((len > seg->size) && fdp_resize_shm(seg, len)) {
        return MAP_FAILED;
    }

    return seg->addr;
}

int
munmap(void *addr, size_t len)
{
    /* wait until underlying "file" is closed */
    return 0;
}
