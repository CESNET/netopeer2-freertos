/**
 * @file emb_posix_sys.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief embedded POSIX library, process, user, time, and logging API
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

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "emb_posix.h"

static char syslog_ident[256] = "";     /**< syslog identity, set by openlog(), cleared by closelog() */
static embp_syslog_cb syslog_cb;        /**< syslog callback called by syslog() and vsyslog() */

void
embp_syslog_set_cb(embp_syslog_cb log_cb)
{
    syslog_cb = log_cb;
}

/* string.h
 */

char *
strdup(const char *s)
{
    char *str;

    str = malloc(strlen(s) + 1);
    if (!str) {
        return NULL;
    }

    strcpy(str, s);
    return str;
}

char *
strndup(const char *s, size_t n)
{
    char *str;
    int len;

    len = strlen(s);
    if (len < n) {
        n = len;
    }

    str = malloc(n + 1);
    if (!str) {
        return NULL;
    }

    strncpy(str, s, n);
    str[n] = '\0';
    return str;
}

/* unistd.h
 */

uid_t
geteuid(void)
{
    return 0;
}

uid_t
getuid(void)
{
    return 0;
}

gid_t
getegid(void)
{
    return 0;
}

long
sysconf(int name)
{
    switch (name) {
    case _SC_GETPW_R_SIZE_MAX:
        return 6;
    case _SC_GETGR_R_SIZE_MAX:
        return 5 + 2 * sizeof(char *);
    default:
        break;
    }

    errno = EINVAL;
    return -1;
}

int
execv(const char *pathname, char *const argv[])
{
    errno = ENOSYS;
    return -1;
}

int
usleep(useconds_t usec)
{
    struct timespec ts;

    ts.tv_sec = usec / 1000000;
    ts.tv_nsec = (usec % 1000000) * 1000;

    nanosleep(&ts, NULL);

    return 0;
}

unsigned int
sleep(unsigned int seconds)
{
    return usleep(seconds * 1000000);
}

int
gethostname(char *name, size_t len)
{
    errno = ENOSYS;
    return -1;
}

int
daemon(int nochdir, int noclose)
{
    return 0;
}

pid_t
getpid(void)
{
    return 0;
}

/* syslog.h
 */

void
openlog(const char *ident, int option, int facility)
{
    strncpy(syslog_ident, ident, sizeof syslog_ident);
}

void
syslog(int priority, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vsyslog(priority, format, ap);
    va_end(ap);
}

void
closelog(void)
{
    syslog_ident[0] = '\0';
}

void
vsyslog(int priority, const char *format, va_list ap)
{
    char *msg;

    if (!syslog_cb) {
        return;
    }

    if (vasprintf(&msg, format, ap) == -1) {
        return;
    }

    syslog_cb(syslog_ident, priority, msg);
    free(msg);
}

/* pwd.h
 */

struct passwd *
getpwnam(const char *name)
{
    static struct passwd pwd;
    static char buf[6];
    struct passwd *result;

    getpwnam_r(name, &pwd, buf, 6, &result);
    return result;
}

struct passwd *
getpwuid(uid_t uid)
{
    if (uid != 0) {
        return NULL;
    }

    return getpwnam("root");
}

int
getpwnam_r(const char *name, struct passwd *pwd, char buf[], size_t buflen, struct passwd **result)
{
    *result = NULL;

    if (buflen < 6) {
        return ERANGE;
    } else if (strcmp(name, "root")) {
        return 0;
    }

    strcpy(buf, "/root");

    /* root user */
    pwd->pw_name = buf + 1;
    pwd->pw_passwd = buf + 1;
    pwd->pw_uid = 0;
    pwd->pw_gid = 0;
    pwd->pw_comment = NULL;
    pwd->pw_gecos = NULL;
    pwd->pw_dir = buf;
    pwd->pw_shell = NULL;

    *result = pwd;
    return 0;
}

int
getpwuid_r(uid_t uid, struct passwd *pwd, char buf[], size_t buflen, struct passwd **result)
{
    *result = NULL;

    if (uid != 0) {
        return 0;
    }

    return getpwnam_r("root", pwd, buf, buflen, result);
}

/* grp.h
 */

struct group *
getgrnam(const char *name)
{
    static struct group grp;
    static char buf[5];
    struct group *result;

    getgrnam_r(name, &grp, buf, 5, &result);
    return result;
}

int
getgrnam_r(const char *name, struct group *grp, char buf[], size_t buflen, struct group **result)
{
    *result = NULL;

    if (buflen < 5) {
        return ERANGE;
    } else if (strcmp(name, "root")) {
        return 0;
    }

    strcpy(buf, "root");

    /* root user */
    grp->gr_name = buf;
    grp->gr_passwd = buf;
    grp->gr_gid = 0;
    grp->gr_mem = calloc(2, sizeof *grp->gr_mem);
    grp->gr_mem[0] = buf;

    *result = grp;
    return 0;
}

int
getgrgid_r(gid_t gid, struct group *grp, char buf[], size_t buflen, struct group **result)
{
    *result = NULL;

    if (gid != 0) {
        return 0;
    }

    return getgrnam_r("root", grp, buf, buflen, result);
}

int
getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups)
{
    if (strcmp(user, "root") || (group != 0)) {
        return -1;
    } else if (*ngroups < 1) {
        *ngroups = 1;
        return -1;
    }

    groups[0] = 0;
    *ngroups = 1;
    return 1;
}

/* sys/wait.h
 */

pid_t
waitpid(pid_t pid, int *wstatus, int options)
{
    pid_t rc;

    if ((pid < -1) || (pid == 0) || options) {
        errno = ENOSYS;
        return -1;
    }

    do {
        rc = wait(wstatus);

        /* wait until the specific process terminates */
    } while ((pid > 0) && (rc != -1) && (rc != pid));

    return rc;
}

/* time.h
 */

time_t
time(time_t *tloc)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_REALTIME, &ts)) {
        return -1;
    }

    if (tloc) {
        *tloc = ts.tv_sec;
    }
    return ts.tv_sec;
}

/* sys/time.h
 */

int
gettimeofday(struct timeval *tv, void *tz)
{
    struct timespec ts;

    if (tz) {
        errno = ENOSYS;
        return -1;
    } else if (!tv) {
        errno = EFAULT;
        return -1;
    }

    if (clock_gettime(CLOCK_REALTIME, &ts)) {
        return -1;
    }

    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec / 1000;
    return 0;
}
