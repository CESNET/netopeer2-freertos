/**
 * @file crypt.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief crypt library
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

#include <crypt.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <mbedtls/md5.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <mbedtls/base64.h>

char *
crypt(const char *phrase, const char *setting)
{
    static struct crypt_data cdata;

    cdata.initialized = 0;

    return crypt_r(phrase, setting, &cdata);
}

char *
crypt_r(const char *phrase, const char *setting, struct crypt_data *data)
{
    mbedtls_md5_context md5_ctx;
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha512_context sha512_ctx;
    size_t base64_len = 0, hash_len;
    const char *salt, *salt_end;

    if (!phrase || !setting || !data || data->initialized) {
        errno = EINVAL;
        return NULL;
    }

    if (!strncmp(setting, "$1$", 3)) {
        /* MD5 */
        mbedtls_md5_init(&md5_ctx);
        mbedtls_md5_starts(&md5_ctx);
        mbedtls_md5_update(&md5_ctx, (unsigned char *)phrase, strlen(phrase));

        salt = setting + 3;
        salt_end = strchr(salt, '$');
        if (!salt_end) {
            salt_end = salt + strlen(salt);
        }
        mbedtls_md5_update(&md5_ctx, (unsigned char *)salt, salt_end - salt);

        mbedtls_md5_finish(&md5_ctx, (unsigned char *)data->setting);
        mbedtls_md5_free(&md5_ctx);

        hash_len = 16;

    } else if (!strncmp(setting, "$5$", 3)) {
        /* SHA256 */
        mbedtls_sha256_init(&sha256_ctx);
        mbedtls_sha256_starts(&sha256_ctx, 0);
        mbedtls_sha256_update(&sha256_ctx, (unsigned char *)phrase, strlen(phrase));

        salt = setting + 3;
        salt_end = strchr(salt, '$');
        if (!salt_end) {
            salt_end = salt + strlen(salt);
        }
        mbedtls_sha256_update(&sha256_ctx, (unsigned char *)salt, salt_end - salt);

        mbedtls_sha256_finish(&sha256_ctx, (unsigned char *)data->setting);
        mbedtls_sha256_free(&sha256_ctx);

        hash_len = 32;

    } else if (!strncmp(setting, "$6$", 3)) {
        /* SHA512 */
        mbedtls_sha512_init(&sha512_ctx);
        mbedtls_sha512_starts(&sha512_ctx, 0);
        mbedtls_sha512_update(&sha512_ctx, (unsigned char *)phrase, strlen(phrase));

        salt = setting + 3;
        salt_end = strchr(salt, '$');
        if (!salt_end) {
            salt_end = salt + strlen(salt);
        }
        mbedtls_sha512_update(&sha512_ctx, (unsigned char *)salt, salt_end - salt);

        mbedtls_sha512_finish(&sha512_ctx, (unsigned char *)data->setting);
        mbedtls_sha512_free(&sha512_ctx);

        hash_len = 128;

    } else {
        /* unsupported algorithm */
        errno = ENOSYS;
        return NULL;
    }

    /* encode using base64 */
    if (mbedtls_base64_encode((unsigned char *)data->output, CRYPT_OUTPUT_SIZE, &base64_len,
            (unsigned char *)data->setting, hash_len)) {
        errno = ERANGE;
        return NULL;
    }

    return data->output;
}
