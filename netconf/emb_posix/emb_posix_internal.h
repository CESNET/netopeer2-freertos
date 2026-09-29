/**
 * @file emb_posix_internal.h
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief embedded POSIX library, shared internal definitions
 *
 * Holds the FD table (FDP) shared by all the emb_posix modules together with
 * the prototypes they use to reach each other.
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

#ifndef EMB_POSIX_INTERNAL_H_
#define EMB_POSIX_INTERNAL_H_

#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <FreeRTOS.h>
#include <stream_buffer.h>

#include "emb_posix_fs.h"
#include "emb_posix_net.h"

#define MAX_FDP_COUNT 80            /**< maximum number of opened FDs */
#define FIFO_BUFFER_SIZE 32 * 1024  /**< size of FIFO buffers */

enum fdp_type {
    FDP_NONE = 0,
    FDP_NET,        /* network socket */
    FDP_FS,         /* file system file */
    FDP_SHM,        /* SHM segment */
    FDP_FIFO        /* (un)named pipe */
};

enum fdp_fs_lock {
    FDP_FS_NONE = 0,
    FDP_FS_WLOCK
};

/**
 * @brief SHM file, member of the global SHM array.
 */
struct fdp_shm {
    char *file_name;
    char *addr;
    size_t size;
    atomic_uint_fast8_t refcount;
};

/**
 * @brief Named or unnamed pipe, member of the global FIFO array.
 */
struct fdp_fifo {
    char *path;         /**< NULL for unnamed pipe */
    StreamBufferHandle_t stream_buf;
    uint8_t *stream_buf_storage;
    StaticStreamBuffer_t stream_buf_struct;
    atomic_uint_fast8_t refcount;
};

/**
 * @brief Single opened FD.
 */
struct fdp_s {
    enum fdp_type type;
    union {
        struct {
            embp_net_sock sock;
        } net;
        struct {
            embp_fs_file file;
            enum fdp_fs_lock lock;
        } fs;
        struct {
            struct fdp_shm *seg;
            uint32_t cur_pos;
            enum fdp_fs_lock lock;
        } shm;
        struct {
            struct fdp_fifo *file;
            int flags;                  /** O_NONBLOCK */
        } fifo;
    };
};

/**
 * @brief Array of all opened FDs.
 */
extern struct fdp_s fdp[MAX_FDP_COUNT];

/**
 * @brief Opened directory, the DIR type.
 */
struct __dirstream {
    enum fdp_type type;
    union {
        embp_fs_dir fs_dir;
        struct {
            char *shm_dir;
            uint32_t next_shm_idx;
        };
    };
};

/* get FDP type of a FD */
#define FDP_TYPE(fd) ((fd > -1) && (fd < MAX_FDP_COUNT) ? fdp[fd].type : FDP_NONE)

/* transform fd into a FILE* */
#define FDP_FD2FILE(fd) ((void *)(uintptr_t)((fd) | 0x80000000))

/* transform FILE* into a fd */
#define FDP_FILE2FD(file) (((uintptr_t)(file)) & ~0x80000000)

/*
 * emb_posix_fdp.c
 */

/**
 * @brief Free a FD.
 *
 * @param[in] fd FD to free.
 */
void fdp_free(int fd);

/**
 * @brief Get a new empty FD.
 *
 * @return New FD, -1 on error.
 */
int fdp_new(void);

/**
 * @brief Store a new network FD.
 *
 * @param[in] sock Backend socket.
 * @return FD number.
 */
int fdp_new_net(embp_net_sock sock);

/**
 * @brief Store a new fs FD.
 *
 * @param[out] file Pointer to the file pointer to use.
 * @return FD number.
 */
int fdp_new_fs(embp_fs_file **file);

/**
 * @brief Get FD equivalent of a backend socket.
 *
 * @param[in] sock Backend socket.
 * @return FD number.
 */
int fdp_sock2fdp(embp_net_sock sock);

/**
 * @brief Check for file name collision with any FDP type.
 *
 * @param[in] path Path of the new file.
 * @return 0 on success.
 * @return -1 on collision, errno set.
 */
int fdp_file_collision(const char *path);

/*
 * emb_posix_shm.c
 */

/**
 * @brief Find a SHM file.
 *
 * @param[in] file_name SHM file name.
 * @return SHM segment, NULL if not found.
 */
struct fdp_shm *fdp_find_shm(const char *file_name);

/**
 * @brief Find the next SHM file in a specific directory.
 *
 * @param[in] dir_name Virtual directory to search.
 * @param[in,out] next_idx Next index to consider, is updated.
 * @return SHM segment, NULL if not found.
 */
struct fdp_shm *fdp_next_shm(const char *dir_name, uint32_t *next_idx);

/**
 * @brief Resize a SHM segment.
 *
 * @param[in] seg SHM segment to modify.
 * @param[in] size New size of the segment.
 * @return 0 on success,
 * @return -1 on error.
 */
int fdp_resize_shm(struct fdp_shm *seg, uint32_t size);

/**
 * @brief Store a new SHM FD.
 *
 * @param[in] file_name SHM file name.
 * @param[in] flags File creation flags.
 * @return FD number.
 */
int fdp_new_shm(const char *file_name, int flags);

/**
 * @brief Free a SHM segment (or decrease its refcount).
 *
 * @param[in] seg SHM segment to free.
 */
void fdp_free_shm(struct fdp_shm *seg);

/**
 * @brief Rename an existing SHM segment.
 *
 * @param[in] old_file_name Current SHM file name.
 * @param[in] new_file_name New SHM file name.
 * @return 0 on success;
 * @return -1 on error, errno set.
 */
int fdp_rename_shm(const char *old_file_name, const char *new_file_name);

/**
 * @brief Read from a SHM FD.
 *
 * @param[in] fd SHM FD.
 * @param[out] buf Buffer to fill.
 * @param[in] count Number of bytes to read.
 * @return Number of bytes read, -1 on error.
 */
ssize_t embp_shm_read(int fd, void *buf, size_t count);

/**
 * @brief Write into a SHM FD.
 *
 * @param[in] fd SHM FD.
 * @param[in] buf Data to write.
 * @param[in] count Number of bytes to write.
 * @return Number of bytes written, -1 on error.
 */
ssize_t embp_shm_write(int fd, const void *buf, size_t count);

/*
 * emb_posix_fifo.c
 */

/**
 * @brief Find a FIFO file (pipe).
 *
 * @param[in] path FIFO file path.
 * @return FIFO file, NULL if not found.
 */
struct fdp_fifo *fdp_find_fifo(const char *path);

/**
 * @brief Create a new FIFO file.
 *
 * @param[in] path FIFO path, may be NULL for an unnamed pipe.
 * @return New FIFO file, NULL on error.
 */
struct fdp_fifo *fdp_new_fifo(const char *path);

/**
 * @brief Free a FIFO file (or decrease its refcount).
 *
 * @param[in] ff FIFO to free.
 */
void fdp_free_fifo(struct fdp_fifo *ff);

/**
 * @brief Read from a FIFO FD.
 *
 * @param[in] fd FIFO FD.
 * @param[out] buf Buffer to fill.
 * @param[in] count Number of bytes to read.
 * @return Number of bytes read, -1 on error.
 */
ssize_t embp_fifo_read(int fd, void *buf, size_t count);

/**
 * @brief Write into a FIFO FD.
 *
 * @param[in] fd FIFO FD.
 * @param[in] buf Data to write.
 * @param[in] count Number of bytes to write.
 * @return Number of bytes written, -1 on error.
 */
ssize_t embp_fifo_write(int fd, const void *buf, size_t count);

/**
 * @brief Poll FIFO FDs.
 *
 * @param[in,out] fds FDs to poll.
 * @param[in] nfds Number of @p fds items.
 * @param[in] timeout Timeout in ms, 0 for none, -1 for infinite.
 * @return Number of FDs with an event.
 */
int embp_fifo_poll(struct pollfd *fds, nfds_t nfds, int timeout);

/*
 * emb_posix_path.c
 */

/**
 * @brief Store the directory of a file as the CWD.
 *
 * @param[in] path File path.
 */
void embp_cwd_set_file(const char *path);

/**
 * @brief Store a directory as the CWD.
 *
 * @param[in] path Directory path.
 */
void embp_cwd_set_dir(const char *path);

#endif /* EMB_POSIX_INTERNAL_H_ */
