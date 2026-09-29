#ifndef _CRYPT_H
#define _CRYPT_H

#define CRYPT_OUTPUT_SIZE 384
#define CRYPT_MAX_PASSPHRASE_SIZE 512

struct crypt_data {
    char output[CRYPT_OUTPUT_SIZE];
    char setting[CRYPT_OUTPUT_SIZE];
    char input[CRYPT_MAX_PASSPHRASE_SIZE];
    char initialized;
};

char *crypt(const char *phrase, const char *setting);

char *crypt_r(const char *phrase, const char *setting, struct crypt_data *data);

#endif /* _CRYPT_H */
