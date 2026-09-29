/**
 * @file emb_posix_dirent.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief embedded POSIX library, directory streams
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
#include <stdlib.h>
#include <string.h>

#ifndef MIN
# define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

struct dirent glob_dirent;

int
closedir(DIR *dirp)
{
    int rc;

    if (!dirp) {
        return 0;
    }

    switch (dirp->type) {
    case FDP_FS:
        rc = embp_fs_closedir(&dirp->fs_dir);
        break;
    case FDP_SHM:
        free(dirp->shm_dir);
        errno = 0;
        rc = 0;
        break;
    default:
        errno = EINTR;
        rc = -1;
        break;
    }

    free(dirp);
    return rc;
}

DIR *
opendir(const char *name)
{
    DIR *dir;

    dir = malloc(sizeof *dir);
    if (!dir) {
        return NULL;
    }

    if (!strcmp(name, "/tmp") || !strncmp(name, "/tmp/", 5)) {
        /* virtual dir */
        dir->type = FDP_SHM;

        dir->shm_dir = strdup(name);
        if (!dir->shm_dir) {
            free(dir);
            errno = ENOMEM;
            return NULL;
        }
        dir->next_shm_idx = 0;

        /* trim trailing '/', if any */
        if (dir->shm_dir[strlen(dir->shm_dir) - 1] == '/') {
            dir->shm_dir[strlen(dir->shm_dir) - 1] = '\0';
        }
        return dir;
    }

    /* FS dir */
    dir->type = FDP_FS;

    if (embp_fs_opendir(&dir->fs_dir, name)) {
        free(dir);
        dir = NULL;
    }

    /* store this file directory as CWD */
    embp_cwd_set_dir(name);

    return dir;
}

struct dirent *
readdir(DIR *dirp)
{
    embp_fs_info info;
    struct fdp_shm *shm;
    int max_len;
    const char *ptr;

    switch (dirp->type) {
    case FDP_FS:
        if (embp_fs_readdir(&dirp->fs_dir, &info) < 1) {
            return NULL;
        }

        /* type */
        glob_dirent.d_type = (info.type == EMBP_FS_TYPE_REG) ? DT_REG : DT_DIR;

        /* name */
        max_len = MIN(255, EMBP_FS_NAME_MAX);
        strncpy(glob_dirent.d_name, info.name, max_len);
        break;
    case FDP_SHM:
        /* skip "/tmp" */
        ptr = dirp->shm_dir + 4;
        if (ptr[0] == '/') {
            ++ptr;
        }

        shm = fdp_next_shm(ptr, &dirp->next_shm_idx);
        if (!shm) {
            /* not found */
            return NULL;
        }

        /* type */
        glob_dirent.d_type = DT_REG;

        /* name */
        max_len = MIN(255, EMBP_FS_NAME_MAX);
        ptr = strrchr(shm->file_name, '/');
        strncpy(glob_dirent.d_name, ptr ? ptr + 1 : shm->file_name, max_len);
        break;
    default:
        errno = EINTR;
        break;
    }

    return &glob_dirent;
}

void
rewinddir(DIR *dirp)
{
    switch (dirp->type) {
    case FDP_FS:
        embp_fs_rewinddir(&dirp->fs_dir);
        break;
    case FDP_SHM:
        dirp->next_shm_idx = 0;
        break;
    default:
        errno = EINTR;
        break;
    }
}

void
seekdir(DIR *dirp, long int loc)
{
    switch (dirp->type) {
    case FDP_FS:
        embp_fs_seekdir(&dirp->fs_dir, loc);
        break;
    case FDP_SHM:
        dirp->next_shm_idx = (uint32_t)loc;
        break;
    default:
        errno = EINTR;
        break;
    }
}

long int
telldir(DIR *dirp)
{
    long int r;

    switch (dirp->type) {
    case FDP_FS:
        r = embp_fs_telldir(&dirp->fs_dir);
        break;
    case FDP_SHM:
        r = (long int)dirp->next_shm_idx;
        break;
    default:
        errno = EINTR;
        r = -1;
        break;
    }

    return r;
}
