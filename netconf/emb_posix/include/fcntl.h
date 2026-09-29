#ifndef _FCNTL_H
#define _FCNTL_H

#include <sys/select.h>

#include "emb_posix_net_sockets.h"

#define O_APPEND    0x08
#define O_CREAT     0x200
#define O_TRUNC     0x400
#define O_EXCL      0x800
#define O_SYNC      0x2000
#define O_CLOEXEC   0x40000
#define O_NOFOLLOW  0x100000

int open(const char *pathname, int flags, ...);

int creat(const char *pathname, mode_t mode);

int fcntl(int fd, int cmd, ...);

/* fcntl(2) requests */
#define F_GETFD     1   /* Get fildes flags (close on exec) */
#define F_SETFD     2   /* Set fildes flags (close on exec) */
#define F_GETFL     3   /* Get file flags */
#define F_SETFL     4   /* Set file flags */

#define F_GETLK     7   /* Get record-locking information */
#define F_SETLK     8   /* Set or Clear a record-lock (Non-Blocking) */
#define F_SETLKW    9   /* Set or Clear a record-lock (Blocking) */

/* fcntl(2) flags (l_type field of flock structure) */
#define F_RDLCK     1   /* read lock */
#define F_WRLCK     2   /* write lock */
#define F_UNLCK     3   /* remove lock(s) */

struct flock {
    short   l_type;     /* F_RDLCK, F_WRLCK, or F_UNLCK */
    short   l_whence;   /* flag to choose starting offset */
    long    l_start;    /* relative offset, in bytes */
    long    l_len;      /* length, in bytes; 0 means lock to EOF */
    short   l_pid;      /* returned with F_GETLK */
    short   l_xxx;      /* reserved for future use */
};

#endif /* _FCNTL_H */
