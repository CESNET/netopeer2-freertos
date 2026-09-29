/**
 * @file emb_posix_stdio.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief embedded POSIX library, buffered streams and formatted output
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
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <FreeRTOS.h>

int
printf(const char *format, ...)
{
    va_list ap;
    int rc;

    va_start(ap, format);
    rc = vfprintf(stdout, format, ap);
    va_end(ap);

    return rc;
}

int
fprintf(FILE *stream, const char *format, ...)
{
    va_list ap;
    int rc;

    va_start(ap, format);
    rc = vfprintf(stream, format, ap);
    va_end(ap);

    return rc;
}

int
vfprintf(FILE *stream, const char *format, va_list ap)
{
    int fd, len, nl;
    char *msg, *msg2, *ptr, *ptr2;
    static pthread_mutex_t stdio_lock = PTHREAD_MUTEX_INITIALIZER;

    if ((stream == stderr) || (stream == stdout)) {
        /* print format message */
        len = vasprintf(&msg, format, ap);
        if (len == -1) {
            PRINTF("Memory allocation failed (%s:%d)\r\n", __func__, __LINE__);
            return -1;
        }

        /* count newlines */
        nl = 0;
        ptr = msg;
        while ((ptr = strchr(ptr, '\n'))) {
            ++nl;
            ++ptr;
        }
        if (!nl) {
            /* print without newlines */
            PRINTF("%s", msg);
            free(msg);
            return len + 1;
        }

        /* alloc */
        msg2 = malloc(len + nl + 1);
        if (!msg2) {
            free(msg);
            PRINTF("Memory allocation failed (%s:%d)\r\n", __func__, __LINE__);
            return -1;
        }

        /* update newlines */
        len = 0;
        ptr = msg;
        while ((ptr2 = strchr(ptr, '\n'))) {
            ++ptr2;

            strncpy(msg2 + len, ptr, ptr2 - ptr);
            len += ptr2 - ptr;

            strcpy(msg2 + len, "\r");
            ++len;

            ptr = ptr2;
        }
        if (*ptr != '\0') {
            strcpy(msg2 + len, ptr);
            len += strlen(ptr);
        }

        /* protected print */
        pthread_mutex_lock(&stdio_lock);
        PRINTF("%s", msg2);
        pthread_mutex_unlock(&stdio_lock);

        /* cleanup */
        free(msg);
        free(msg2);
        return len + 1;
    }

    fd = FDP_FILE2FD(stream);
    return vdprintf(fd, format, ap);
}

int
vdprintf(int fd, const char *format, va_list ap)
{
    int rc, len;
    char *buf;

    len = vasprintf(&buf, format, ap);
    if (len == -1) {
        return -1;
    }

    rc = write(fd, buf, len);
    free(buf);
    return rc;
}

int
asprintf(char **strp, const char *fmt, ...)
{
    va_list va;
    int len;

    va_start(va, fmt);
    len = vasprintf(strp, fmt, va);
    va_end(va);

    return len;
}

int
vasprintf(char **strp, const char *fmt, va_list ap)
{
    va_list va_copy;
    int len;

    *strp = NULL;

    /* get final string length */
    va_copy(va_copy, ap);
    len = vsnprintf(*strp, 0, fmt, va_copy);
    va_end(va_copy);

    *strp = malloc(len + 1);
    if (!*strp) {
        return -1;
    }

    /* print the string */
    return vsnprintf(*strp, len + 1, fmt, ap);
}

FILE *
fopen(const char *pathname, const char *mode)
{
    int fd, flags = 0;

    if ((mode[0] == 'w') || (mode[0] == 'a')) {
        flags |= O_CREAT;
    }

    fd = open(pathname, flags);
    if (fd == -1) {
        return NULL;
    }

    /* fd 0 must not be interpreted as NULL */
    return FDP_FD2FILE(fd);
}

FILE *
fdopen(int fd, const char *mode)
{
    return FDP_FD2FILE(fd);
}

size_t
fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    int fd = FDP_FILE2FD(stream);

    return read(fd, ptr, nmemb * size);
}

size_t
fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    int fd, len, nl;

    if ((stream == stderr) || (stream == stdout)) {
        len = size * nmemb;
        nl = (((char *)ptr)[len - 1] == '\n') ? 1 : 0;

        PRINTF("%.*s%s", len, ptr, nl ? "\r" : "");
        return len + nl;
    }

    fd = FDP_FILE2FD(stream);
    return write(fd, ptr, nmemb * size);
}

int
fclose(FILE *stream)
{
    int fd = FDP_FILE2FD(stream);

    return close(fd);
}

int
fflush(FILE *stream)
{
    int fd = FDP_FILE2FD(stream);

    return fsync(fd);
}

int
fileno(FILE *stream)
{
    return FDP_FILE2FD(stream);
}

int
fgetc(FILE *stream)
{
    int fd = FDP_FILE2FD(stream);
    ssize_t r;
    unsigned char c;

    r = read(fd, &c, 1);

    return (r == 1) ? c : EOF;
}

int
getc(FILE *stream)
{
    return fgetc(stream);
}

char *
fgets(char s[], int size, FILE *stream)
{
    int fd = FDP_FILE2FD(stream), rr;
    ssize_t r;

    rr = 0;
    do {
        /* read a char */
        r = read(fd, &s[rr], 1);
        if (r == -1) {
            /* error */
            return NULL;
        } else if (r == 0) {
            /* EOF */
            break;
        }

        ++rr;
    } while ((s[rr] != '\n') && (rr < size));

    if (rr) {
        s[rr + 1] = '\0';
        return s;
    } else {
        return NULL;
    }
}

int
fseek(FILE *stream, long offset, int whence)
{
    int fd = FDP_FILE2FD(stream);

    return lseek(fd, offset, whence);
}

long
ftell(FILE *stream)
{
    int fd = FDP_FILE2FD(stream);

    return lseek(fd, 0, SEEK_CUR);
}

void
rewind(FILE *stream)
{
    fseek(stream, 0, SEEK_SET);
}

int
fputc(int c, FILE *stream)
{
    if (fwrite((unsigned char *)&c, 1, 1, stream) != 1) {
        return EOF;
    }

    return c;
}

int
putc(int c, FILE *stream)
{
    return fputc(c, stream);
}

int
putchar(int c)
{
    return fputc(c, stdout);
}

int
fputs(const char *s, FILE *stream)
{
    int len = strlen(s);

    if (fwrite(s, 1, len, stream) != len) {
        return EOF;
    }

    return len;
}

int
puts(const char *s)
{
    return fprintf(stdout, "%s\n", s);
}
