/**
 * @file crypt.c
 * @author Petr Hanzlik <Petr.Hanzlik@cesnet.cz>
 * @brief crypt library - crypt(3)-compatible password hashing
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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mbedtls/md.h>
#include <mbedtls/platform_util.h>

/** salt length limits defined by the formats */
#define CRYPT_MD5_SALT_MAX 8
#define CRYPT_SHA_SALT_MAX 16

/** SHA-crypt "rounds=" limits, part of the format */
#define CRYPT_SHA_ROUNDS_MIN UINT32_C(1000)
#define CRYPT_SHA_ROUNDS_MAX UINT32_C(999999999)
#define CRYPT_SHA_ROUNDS_DEFAULT UINT32_C(5000)

/** number of MD5-crypt stretching rounds, fixed by the format */
#define CRYPT_MD5_ROUNDS UINT32_C(1000)

/** lengths of the base64-encoded digests */
#define CRYPT_MD5_HASH_LEN 22
#define CRYPT_SHA256_HASH_LEN 43
#define CRYPT_SHA512_HASH_LEN 86

/**
 * @brief Digest algorithms of the supported schemes.
 */
enum crypt_alg {
    CRYPT_ALG_MD5,     /**< MD5, 16-byte digest */
    CRYPT_ALG_SHA256,  /**< SHA-256, 32-byte digest */
    CRYPT_ALG_SHA512   /**< SHA-512, 64-byte digest */
};

/**
 * @brief Digest context for one streaming hash computation.
 */
struct crypt_digest {
    enum crypt_alg alg;         /**< algorithm to compute, the same for the whole life of the context */
    mbedtls_md_context_t *ctx;  /* backend context, NULL until the first init */
};

/**
 * @brief Start (or restart) a digest computation.
 *
 * The context must always be used with the same algorithm.
 *
 * @param[in,out] digest_ctx Digest with ::alg set. May be reused after a final.
 * @return 0 on success, non-zero on failure.
 */
static int
crypt_digest_init(struct crypt_digest *digest_ctx)
{
    const mbedtls_md_info_t *info;
    mbedtls_md_type_t type;

    /* get the mbed TLS digest for the algorithm */
    switch (digest_ctx->alg) {
    case CRYPT_ALG_MD5:
        type = MBEDTLS_MD_MD5;
        break;
    case CRYPT_ALG_SHA256:
        type = MBEDTLS_MD_SHA256;
        break;
    case CRYPT_ALG_SHA512:
        type = MBEDTLS_MD_SHA512;
        break;
    default:
        errno = EIO;
        return 1;
    }
    info = mbedtls_md_info_from_type(type);
    if (!info) {
        errno = EIO;
        return 1;
    }

    /* on the first use, allocate and set up the backend context */
    if (!digest_ctx->ctx) {
        digest_ctx->ctx = calloc(1, sizeof *digest_ctx->ctx);
        if (!digest_ctx->ctx) {
            errno = ENOMEM;
            return 1;
        }
        if (mbedtls_md_setup(digest_ctx->ctx, info, 0)) {
            free(digest_ctx->ctx);
            digest_ctx->ctx = NULL;
            errno = EIO;
            return 1;
        }
    }

    /* (re)start the computation, the context is reused between the rounds */
    if (mbedtls_md_starts(digest_ctx->ctx)) {
        errno = EIO;
        return 1;
    }

    return 0;
}

/**
 * @brief Append data to a digest computation.
 *
 * @param[in,out] digest_ctx Initialized digest.
 * @param[in] data Bytes to hash.
 * @param[in] len Number of bytes.
 * @return 0 on success, non-zero on failure.
 */
static int
crypt_digest_update(struct crypt_digest *digest_ctx, const void *data, size_t len)
{
    if (mbedtls_md_update(digest_ctx->ctx, data, len)) {
        errno = EIO;
        return 1;
    }

    return 0;
}

/**
 * @brief Finish the computation and write the digest bytes.
 *
 * @param[in,out] digest_ctx Initialized digest.
 * @param[out] out Buffer for the digest bytes.
 * @return 0 on success, non-zero on failure.
 */
static int
crypt_digest_final(struct crypt_digest *digest_ctx, void *out)
{
    if (mbedtls_md_finish(digest_ctx->ctx, out)) {
        errno = EIO;
        return 1;
    }

    return 0;
}

/**
 * @brief Free all resources of a digest.
 *
 * @param[in,out] digest_ctx Digest to clean up.
 */
static void
crypt_digest_cleanup(struct crypt_digest *digest_ctx)
{
    if (digest_ctx->ctx) {
        mbedtls_md_free(digest_ctx->ctx);
        free(digest_ctx->ctx);
        digest_ctx->ctx = NULL;
    }
}

/**
 * @brief Base64 alphabet shared by the hashing schemes.
 */
static const char crypt_b64tab[] =
        "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

/**
 * @brief Parsed setting string.
 */
struct crypt_setting {
    enum crypt_alg alg;                  /**< digest algorithm */
    uint32_t rounds;                     /**< SHA-crypt rounds */
    int rounds_custom;                   /**< "rounds=" was present in the setting */
    char salt[CRYPT_SHA_SALT_MAX + 1];   /**< salt, truncated and NUL-terminated */
    size_t salt_len;                     /**< salt length */
};

/**
 * @brief Check that a string consists only of the crypt base64 alphabet characters.
 *
 * @param[in] str String to check.
 * @param[in] len Length of @p str.
 * @return 1 if all the characters are valid, 0 otherwise.
 */
static int
crypt_valid_b64(const char *str, size_t len)
{
    size_t i;

    for (i = 0; i < len; ++i) {
        if (!memchr(crypt_b64tab, str[i], sizeof crypt_b64tab - 1)) {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief Parse a setting string "$<id>$[rounds=N$]<salt>[$<digest>]".
 *
 * A digest stored in the setting is ignored, only the salt (and rounds) is used
 * for the hash computation, as crypt(3) does.
 *
 * @param[in] setting Setting to parse.
 * @param[out] set Parsed setting.
 * @return 0 on success, -1 on error (errno set).
 */
static int
crypt_parse_setting(const char *setting, struct crypt_setting *set)
{
    const char *p;
    char *end = NULL;
    uint64_t rounds;
    size_t salt_run_len, salt_max;

    if ((setting[0] != '$') || !setting[1] || !setting[2]) {
        errno = EINVAL;
        return -1;
    }
    if (setting[2] != '$') {
        errno = EINVAL;
        return -1;
    }

    /* parse the hash id */
    switch (setting[1]) {
    case '1':
        set->alg = CRYPT_ALG_MD5;
        set->rounds = CRYPT_MD5_ROUNDS;
        salt_max = CRYPT_MD5_SALT_MAX;
        break;
    case '5':
        set->alg = CRYPT_ALG_SHA256;
        set->rounds = CRYPT_SHA_ROUNDS_DEFAULT;
        salt_max = CRYPT_SHA_SALT_MAX;
        break;
    case '6':
        set->alg = CRYPT_ALG_SHA512;
        set->rounds = CRYPT_SHA_ROUNDS_DEFAULT;
        salt_max = CRYPT_SHA_SALT_MAX;
        break;
    default:
        errno = ENOSYS;
        return -1;
    }

    p = setting + 3;

    /* optional "rounds=<N>$" prefix of the SHA-crypt schemes */
    if (((set->alg == CRYPT_ALG_SHA256) || (set->alg == CRYPT_ALG_SHA512)) &&
            !strncmp(p, "rounds=", 7)) {

        rounds = strtoul(p + 7, &end, 10);
        if ((end == (p + 7)) || (*end != '$')) {
            errno = EINVAL;
            return -1;
        }
        if ((rounds < CRYPT_SHA_ROUNDS_MIN) || (rounds > CRYPT_SHA_ROUNDS_MAX)) {
            errno = EINVAL;
            return -1;
        }
        /* the value was validated, the truncation cannot wrap it into the valid range */
        set->rounds = (uint32_t)rounds;
        set->rounds_custom = 1;
        p = end + 1;
    }

    /* parse the salt, anything after it (a stored digest) is ignored */
    salt_run_len = strcspn(p, "$");
    if ((salt_run_len > 0) && !crypt_valid_b64(p, salt_run_len)) {
        errno = EINVAL;
        return -1;
    }
    set->salt_len = (salt_run_len < salt_max) ? salt_run_len : salt_max;
    memcpy(set->salt, p, set->salt_len);
    set->salt[set->salt_len] = '\0';

    return 0;
}

/**
 * @brief Encode 3 bytes as up to 4 base64 characters.
 *
 * The 24 bits are consumed 6 at a time starting from the least significant
 * bit.
 *
 * @param[in,out] out Output position, advanced by @p nchars characters.
 * @param[in] b2 First byte, most significant in the 24-bit word.
 * @param[in] b1 Second byte.
 * @param[in] b0 Third byte, least significant in the 24-bit word.
 * @param[in] nchars Number of characters to emit (2-4).
 */
static void
crypt_b64_group(char **out, unsigned char b2, unsigned char b1, unsigned char b0, int nchars)
{
    /* assemble one 24-bit word from the 3 bytes, b2 is the most significant
     * byte (the encode tables pass the first digest byte of a group as b2) */
    uint32_t w = ((uint32_t)b2 << 16) | ((uint32_t)b1 << 8) | b0;

    /* emit one character per 6 bits starting from the least significant bits,
     * the reverse of the standard base64 bit order; the final partial groups
     * pass their leftover bytes as b0/b1 so that all the significant bits
     * are covered by the emitted characters */
    while (nchars-- > 0) {
        *(*out)++ = crypt_b64tab[w & 0x3f];
        w >>= 6;
    }
}

/**
 * @brief Encode a raw digest in the base64 variant of the given scheme.
 *
 * @param[in] alg Algorithm of the digest.
 * @param[in] digest Raw digest bytes.
 * @param[out] out Buffer for the encoded digest, NUL-terminated by the caller.
 */
static void
crypt_encode(enum crypt_alg alg, const unsigned char *digest, char *out)
{
    switch (alg) {
    case CRYPT_ALG_MD5:
        crypt_b64_group(&out, digest[0], digest[6], digest[12], 4);
        crypt_b64_group(&out, digest[1], digest[7], digest[13], 4);
        crypt_b64_group(&out, digest[2], digest[8], digest[14], 4);
        crypt_b64_group(&out, digest[3], digest[9], digest[15], 4);
        crypt_b64_group(&out, digest[4], digest[10], digest[5], 4);
        crypt_b64_group(&out, 0, 0, digest[11], 2);
        break;
    case CRYPT_ALG_SHA256:
        crypt_b64_group(&out, digest[0], digest[10], digest[20], 4);
        crypt_b64_group(&out, digest[21], digest[1], digest[11], 4);
        crypt_b64_group(&out, digest[12], digest[22], digest[2], 4);
        crypt_b64_group(&out, digest[3], digest[13], digest[23], 4);
        crypt_b64_group(&out, digest[24], digest[4], digest[14], 4);
        crypt_b64_group(&out, digest[15], digest[25], digest[5], 4);
        crypt_b64_group(&out, digest[6], digest[16], digest[26], 4);
        crypt_b64_group(&out, digest[27], digest[7], digest[17], 4);
        crypt_b64_group(&out, digest[18], digest[28], digest[8], 4);
        crypt_b64_group(&out, digest[9], digest[19], digest[29], 4);
        crypt_b64_group(&out, 0, digest[31], digest[30], 3);
        break;
    case CRYPT_ALG_SHA512:
        crypt_b64_group(&out, digest[0], digest[21], digest[42], 4);
        crypt_b64_group(&out, digest[22], digest[43], digest[1], 4);
        crypt_b64_group(&out, digest[44], digest[2], digest[23], 4);
        crypt_b64_group(&out, digest[3], digest[24], digest[45], 4);
        crypt_b64_group(&out, digest[25], digest[46], digest[4], 4);
        crypt_b64_group(&out, digest[47], digest[5], digest[26], 4);
        crypt_b64_group(&out, digest[6], digest[27], digest[48], 4);
        crypt_b64_group(&out, digest[28], digest[49], digest[7], 4);
        crypt_b64_group(&out, digest[50], digest[8], digest[29], 4);
        crypt_b64_group(&out, digest[9], digest[30], digest[51], 4);
        crypt_b64_group(&out, digest[31], digest[52], digest[10], 4);
        crypt_b64_group(&out, digest[53], digest[11], digest[32], 4);
        crypt_b64_group(&out, digest[12], digest[33], digest[54], 4);
        crypt_b64_group(&out, digest[34], digest[55], digest[13], 4);
        crypt_b64_group(&out, digest[56], digest[14], digest[35], 4);
        crypt_b64_group(&out, digest[15], digest[36], digest[57], 4);
        crypt_b64_group(&out, digest[37], digest[58], digest[16], 4);
        crypt_b64_group(&out, digest[59], digest[17], digest[38], 4);
        crypt_b64_group(&out, digest[18], digest[39], digest[60], 4);
        crypt_b64_group(&out, digest[40], digest[61], digest[19], 4);
        crypt_b64_group(&out, digest[62], digest[20], digest[41], 4);
        crypt_b64_group(&out, 0, 0, digest[63], 2);
        break;
    }
}

/**
 * @brief One operand of a stretching round.
 */
struct crypt_feed {
    const void *data; /**< operand bytes */
    size_t len;       /**< operand length */
};

/**
 * @brief Run the stretching rounds.
 *
 * @param[in,out] digest_ctx Digest context with ::alg set, reused every round.
 * @param[in] pw_seq Password-derived byte sequence.
 * @param[in] pw_seq_len Length of @p pw_seq.
 * @param[in] salt_seq Salt-derived byte sequence.
 * @param[in] salt_seq_len Length of @p salt_seq.
 * @param[in,out] digest Digest from the preliminary phase, replaced every round.
 * @param[in] digest_len Digest length.
 * @param[in] rounds Number of rounds.
 * @return 0 on success, -1 on digest failure (errno set).
 */
static int
crypt_stretch(struct crypt_digest *digest_ctx, const void *pw_seq, size_t pw_seq_len,
        const void *salt_seq, size_t salt_seq_len, unsigned char *digest, size_t digest_len,
        uint32_t rounds)
{
    struct crypt_feed feeds[4];
    uint32_t n, j;
    size_t i;

    for (i = 0; i < rounds; ++i) {
        feeds[0].data = (i & 1) ? pw_seq : digest;
        feeds[0].len = (i & 1) ? pw_seq_len : digest_len;

        n = 1;
        if (i % 3) {
            feeds[n].data = salt_seq;
            feeds[n].len = salt_seq_len;
            ++n;
        }
        if (i % 7) {
            feeds[n].data = pw_seq;
            feeds[n].len = pw_seq_len;
            ++n;
        }

        feeds[n].data = (i & 1) ? digest : pw_seq;
        feeds[n].len = (i & 1) ? digest_len : pw_seq_len;
        ++n;

        if (crypt_digest_init(digest_ctx)) {
            return -1;
        }
        for (j = 0; j < n; ++j) {
            if (crypt_digest_update(digest_ctx, feeds[j].data, feeds[j].len)) {
                return -1;
            }
        }
        if (crypt_digest_final(digest_ctx, digest)) {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Compute the raw MD5-crypt ("$1$") digest.
 *
 * @param[in] password Password.
 * @param[in] pw_len Password length.
 * @param[in] salt Salt, already truncated.
 * @param[in] salt_len Salt length.
 * @param[out] digest Resulting 16 bytes.
 * @return 0 on success, -1 on digest failure (errno set).
 */
static int
crypt_md5_digest(const char *password, size_t pw_len, const char *salt, size_t salt_len,
        unsigned char digest[16])
{
    struct crypt_digest digest_ctx = {0};
    unsigned char alt[16] = {0};
    int rc = 0;
    size_t n, i;

    digest_ctx.alg = CRYPT_ALG_MD5;

    /* alternate sum B (password + salt + password) */
    if (crypt_digest_init(&digest_ctx) ||
            crypt_digest_update(&digest_ctx, password, pw_len) ||
            crypt_digest_update(&digest_ctx, salt, salt_len) ||
            crypt_digest_update(&digest_ctx, password, pw_len) ||
            crypt_digest_final(&digest_ctx, alt)) {
        rc = -1;
        goto cleanup;
    }

    /* open digest A (password + "$1$" + salt) */
    if (crypt_digest_init(&digest_ctx) ||
            crypt_digest_update(&digest_ctx, password, pw_len) ||
            crypt_digest_update(&digest_ctx, "$1$", 3) ||
            crypt_digest_update(&digest_ctx, salt, salt_len)) {
        rc = -1;
        goto cleanup;
    }

    /* for every full 16 bytes of password length add B, then the first n bytes */
    for (n = pw_len; n > 16; n -= 16) {
        if (crypt_digest_update(&digest_ctx, alt, 16)) {
            rc = -1;
            goto cleanup;
        }
    }
    if (crypt_digest_update(&digest_ctx, alt, n)) {
        rc = -1;
        goto cleanup;
    }

    /* for every set bit of the password length a NUL byte, otherwise the first password byte */
    for (i = pw_len; i; i >>= 1) {
        if (crypt_digest_update(&digest_ctx, (i & 1) ? "" : password, 1)) {
            rc = -1;
            goto cleanup;
        }
    }

    if (crypt_digest_final(&digest_ctx, digest)) {
        rc = -1;
        goto cleanup;
    }

    rc = crypt_stretch(&digest_ctx, password, pw_len, salt, salt_len, digest, 16, CRYPT_MD5_ROUNDS);

cleanup:
    /* the alternate sum is derived from the password */
    mbedtls_platform_zeroize(alt, sizeof alt);
    crypt_digest_cleanup(&digest_ctx);
    return rc;
}

/**
 * @brief Compute the raw SHA-crypt ("$5$"/"$6$") digest.
 *
 * @param[in] alg SHA256 or SHA512.
 * @param[in] password Password.
 * @param[in] pw_len Password length.
 * @param[in] salt Salt, already truncated.
 * @param[in] salt_len Salt length.
 * @param[in] rounds Number of rounds.
 * @param[out] digest Resulting 32 or 64 bytes.
 * @return 0 on success, -1 on digest failure (errno set).
 */
static int
crypt_sha_digest(enum crypt_alg alg, const char *password, size_t pw_len,
        const char *salt, size_t salt_len, uint32_t rounds, unsigned char *digest)
{
    struct crypt_digest digest_ctx = {0};
    unsigned char alt[64] = {0}, dp[64] = {0}, ds[64] = {0};
    unsigned char *p_seq = NULL, *s_seq = NULL;
    size_t n, size, i;
    int rc = 0;

    size = (alg == CRYPT_ALG_SHA256) ? 32 : 64;
    digest_ctx.alg = alg;

    /* alternate sum B = SHA(password + salt + password) */
    if (crypt_digest_init(&digest_ctx) ||
            crypt_digest_update(&digest_ctx, password, pw_len) ||
            crypt_digest_update(&digest_ctx, salt, salt_len) ||
            crypt_digest_update(&digest_ctx, password, pw_len) ||
            crypt_digest_final(&digest_ctx, alt)) {
        rc = -1;
        goto cleanup;
    }

    /* open digest A (password + salt) */
    if (crypt_digest_init(&digest_ctx) ||
            crypt_digest_update(&digest_ctx, password, pw_len) ||
            crypt_digest_update(&digest_ctx, salt, salt_len)) {
        rc = -1;
        goto cleanup;
    }

    /* for every full digest-sized block of password length add B, then the first n bytes */
    for (n = pw_len; n > size; n -= size) {
        if (crypt_digest_update(&digest_ctx, alt, size)) {
            rc = -1;
            goto cleanup;
        }
    }
    if (crypt_digest_update(&digest_ctx, alt, n)) {
        rc = -1;
        goto cleanup;
    }

    /* for every set bit of the password length the whole B, otherwise the whole password */
    for (i = pw_len; i; i >>= 1) {
        if (i & 1) {
            if (crypt_digest_update(&digest_ctx, alt, size)) {
                rc = -1;
                goto cleanup;
            }
        } else if (crypt_digest_update(&digest_ctx, password, pw_len)) {
            rc = -1;
            goto cleanup;
        }
    }

    if (crypt_digest_final(&digest_ctx, digest)) {
        rc = -1;
        goto cleanup;
    }

    /* password precompression DP = SHA(password for every byte of the password) */
    if (crypt_digest_init(&digest_ctx)) {
        rc = -1;
        goto cleanup;
    }
    for (i = 0; i < pw_len; ++i) {
        if (crypt_digest_update(&digest_ctx, password, pw_len)) {
            rc = -1;
            goto cleanup;
        }
    }
    if (crypt_digest_final(&digest_ctx, dp)) {
        rc = -1;
        goto cleanup;
    }

    /* salt precompression DS = SHA(salt repeated 16 + digest[0] times) */
    if (crypt_digest_init(&digest_ctx)) {
        rc = -1;
        goto cleanup;
    }
    for (i = 0; i < (size_t)(16 + digest[0]); ++i) {
        if (crypt_digest_update(&digest_ctx, salt, salt_len)) {
            rc = -1;
            goto cleanup;
        }
    }
    if (crypt_digest_final(&digest_ctx, ds)) {
        rc = -1;
        goto cleanup;
    }

    /* P and S byte sequences: DP and DS cycled to the password and salt lengths */
    p_seq = calloc(pw_len ? pw_len : 1, sizeof *p_seq);
    if (p_seq) {
        s_seq = calloc(salt_len ? salt_len : 1, sizeof *s_seq);
    }
    if (!p_seq || !s_seq) {
        errno = ENOMEM;
        rc = -1;
        goto cleanup;
    }
    for (i = 0; i < pw_len; ++i) {
        p_seq[i] = dp[i % size];
    }
    for (i = 0; i < salt_len; ++i) {
        s_seq[i] = ds[i % size];
    }

    rc = crypt_stretch(&digest_ctx, p_seq, pw_len, s_seq, salt_len, digest, size, rounds);

cleanup:
    /* P, S and the alternate sums are derived from the password */
    if (p_seq) {
        mbedtls_platform_zeroize(p_seq, pw_len ? pw_len : 1);
    }
    if (s_seq) {
        mbedtls_platform_zeroize(s_seq, salt_len ? salt_len : 1);
    }
    free(p_seq);
    free(s_seq);
    mbedtls_platform_zeroize(alt, sizeof alt);
    mbedtls_platform_zeroize(dp, sizeof dp);
    mbedtls_platform_zeroize(ds, sizeof ds);
    crypt_digest_cleanup(&digest_ctx);
    return rc;
}

/**
 * @brief Create a password hash in the "$<id>$[rounds=N$]<salt>$<digest>" format.
 *
 * @param[in] password Clear-text password.
 * @param[in] setting Setting string such as "$6$[rounds=N$]<salt>$", any digest
 * stored in it after the salt is ignored.
 * @param[out] output Buffer for the created hash.
 * @param[in] output_len Length of the buffer including the terminating NUL.
 * @return 0 on success, -1 on error (errno set).
 */
static int
crypt_create(const char *password, const char *setting, char *output, size_t output_len)
{
    struct crypt_setting set = {0};
    unsigned char digest[64] = {0};
    char b64[CRYPT_SHA512_HASH_LEN + 1] = {0};
    size_t pw_len, hash_len = 0, needed, digits;
    char id;
    int rc;

    rc = crypt_parse_setting(setting, &set);
    if (rc) {
        return rc;
    }

    /* learn the encoded digest length */
    switch (set.alg) {
    case CRYPT_ALG_MD5:
        hash_len = CRYPT_MD5_HASH_LEN;
        break;
    case CRYPT_ALG_SHA256:
        hash_len = CRYPT_SHA256_HASH_LEN;
        break;
    case CRYPT_ALG_SHA512:
        hash_len = CRYPT_SHA512_HASH_LEN;
        break;
    }

    /* validate the output parameters */
    id = (set.alg == CRYPT_ALG_MD5) ? '1' : (set.alg == CRYPT_ALG_SHA256) ? '5' : '6';

    needed = 3 + set.salt_len + 1 + hash_len + 1;
    if (set.rounds_custom) {
        needed += 8; /* "rounds=" and the trailing '$' */
        digits = set.rounds;
        do {
            ++needed;
            digits /= 10;
        } while (digits);
    }

    if (output_len < needed) {
        errno = ERANGE;
        return -1;
    }

    pw_len = strlen(password);

    if (set.alg == CRYPT_ALG_MD5) {
        rc = crypt_md5_digest(password, pw_len, set.salt, set.salt_len, digest);
    } else {
        rc = crypt_sha_digest(set.alg, password, pw_len, set.salt, set.salt_len, set.rounds, digest);
    }
    if (rc) {
        /* the digest may be partially computed */
        mbedtls_platform_zeroize(digest, sizeof digest);
        return rc;
    }

    crypt_encode(set.alg, digest, b64);
    b64[hash_len] = '\0';

    if (set.rounds_custom) {
        snprintf(output, output_len, "$%c$rounds=%" PRIu32 "$%s$%s", id, set.rounds, set.salt, b64);
    } else {
        snprintf(output, output_len, "$%c$%s$%s", id, set.salt, b64);
    }

    /* the digest and its encoding are derived from the password */
    mbedtls_platform_zeroize(digest, sizeof digest);
    mbedtls_platform_zeroize(b64, sizeof b64);

    return 0;
}

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
    if (!phrase || !setting || !data || data->initialized) {
        errno = EINVAL;
        return NULL;
    }

    if (crypt_create(phrase, setting, data->output, CRYPT_OUTPUT_SIZE)) {
        return NULL;
    }

    return data->output;
}
