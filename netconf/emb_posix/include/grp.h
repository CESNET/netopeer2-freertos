#ifndef _GRP_H_
#define _GRP_H_

#include <sys/types.h>

struct group {
    char *gr_name;      /* group name */
    char *gr_passwd;    /* group password */
    gid_t gr_gid;       /* group id */
    char **gr_mem;      /* group members */
};

struct group *getgrnam(const char *name);

int getgrnam_r(const char *name, struct group *grp, char buf[], size_t buflen, struct group **result);
int getgrgid_r(gid_t gid, struct group *grp, char buf[], size_t buflen, struct group **result);

int getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups);

#endif /* _GRP_H_ */
