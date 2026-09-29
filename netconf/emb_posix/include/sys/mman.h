#ifndef _SYS_MMAN_H_
#define _SYS_MMAN_H_

#include <stddef.h>
#include <sys/types.h>

/* Protections are chosen from these bits, OR'd together.  The
   implementation does not necessarily support PROT_EXEC or PROT_WRITE
   without PROT_READ.  The only guarantees are that no writing will be
   allowed without PROT_WRITE and no access will be allowed for PROT_NONE. */

#define PROT_READ       0x1             /* Page can be read.  */
#define PROT_WRITE      0x2             /* Page can be written.  */
#define PROT_EXEC       0x4             /* Page can be executed.  */
#define PROT_NONE       0x0             /* Page can not be accessed.  */

/* Sharing types (must choose one and only one of these).  */
#define MAP_SHARED      0x01            /* Share changes.  */
#define MAP_PRIVATE     0x02            /* Changes are private.  */
#define MAP_SHARED_VALIDATE     0x03    /* Share changes and validate
                                           extension flags.  */
#define MAP_TYPE        0x0f            /* Mask for type of mapping.  */

/* Other flags.  */
#define MAP_FIXED       0x10            /* Interpret addr exactly.  */
#define MAP_FILE        0
#define MAP_ANONYMOUS   0x20            /* Don't use a file.  */
#define MAP_ANON        MAP_ANONYMOUS
/* When MAP_HUGETLB is set bits [26:31] encode the log2 of the huge page size.  */
#define MAP_HUGE_SHIFT  26
#define MAP_HUGE_MASK   0x3f

/* Return value of `mmap' in case of an error.  */
#define MAP_FAILED      ((void *) -1)

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);

int munmap(void *addr, size_t len);

#endif
