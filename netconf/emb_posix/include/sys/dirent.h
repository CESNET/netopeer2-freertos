#ifndef _DIRENT_H
#define _DIRENT_H

/* This file defines `struct dirent'.

   It defines the macro `_DIRENT_HAVE_D_NAMLEN' iff there is a `d_namlen'
   member that gives the length of `d_name'.

   It defines the macro `_DIRENT_HAVE_D_RECLEN' iff there is a `d_reclen'
   member that gives the size of the entire directory entry.

   It defines the macro `_DIRENT_HAVE_D_OFF' iff there is a `d_off'
   member that gives the file offset of the next directory entry.

   It defines the macro `_DIRENT_HAVE_D_TYPE' iff there is a `d_type'
   member that gives the type of the file.
 */

#include <sys/types.h>

struct dirent
  {
    ino_t d_ino;
    off_t d_off;
    unsigned short int d_reclen;
    unsigned char d_type;
    char d_name[256];           /* We must not include limits.h! */
  };

#define d_fileno        d_ino   /* Backwards compatibility.  */

#undef  _DIRENT_HAVE_D_NAMLEN
#define _DIRENT_HAVE_D_RECLEN
#define _DIRENT_HAVE_D_OFF
#define _DIRENT_HAVE_D_TYPE

#ifndef d_fileno
# define d_ino  d_fileno                 /* Backward compatibility.  */
#endif

/* These macros extract size information from a `struct dirent *'.
   They may evaluate their argument multiple times, so it must not
   have side effects.  Each of these may involve a relatively costly
   call to `strlen' on some systems, so these values should be cached.

   _D_EXACT_NAMLEN (DP) returns the length of DP->d_name, not including
   its terminating null character.

   _D_ALLOC_NAMLEN (DP) returns a size at least (_D_EXACT_NAMLEN (DP) + 1);
   that is, the allocation size needed to hold the DP->d_name string.
   Use this macro when you don't need the exact length, just an upper bound.
   This macro is less likely to require calling `strlen' than _D_EXACT_NAMLEN.
   */

#ifdef _DIRENT_HAVE_D_NAMLEN
# define _D_EXACT_NAMLEN(d) ((d)->d_namlen)
# define _D_ALLOC_NAMLEN(d) (_D_EXACT_NAMLEN (d) + 1)
#else
# define _D_EXACT_NAMLEN(d) (strlen ((d)->d_name))
# ifdef _DIRENT_HAVE_D_RECLEN
#  define _D_ALLOC_NAMLEN(d) (((char *) (d) + (d)->d_reclen) - &(d)->d_name[0])
# else
#  define _D_ALLOC_NAMLEN(d) (sizeof (d)->d_name > 1 ? sizeof (d)->d_name \
                              : _D_EXACT_NAMLEN (d) + 1)
# endif
#endif


/* File types for `d_type'.  */
enum
  {
    DT_UNKNOWN = 0,
# define DT_UNKNOWN     DT_UNKNOWN
    DT_FIFO = 1,
# define DT_FIFO        DT_FIFO
    DT_CHR = 2,
# define DT_CHR         DT_CHR
    DT_DIR = 4,
# define DT_DIR         DT_DIR
    DT_BLK = 6,
# define DT_BLK         DT_BLK
    DT_REG = 8,
# define DT_REG         DT_REG
    DT_LNK = 10,
# define DT_LNK         DT_LNK
    DT_SOCK = 12,
# define DT_SOCK        DT_SOCK
    DT_WHT = 14
# define DT_WHT         DT_WHT
  };

/* Convert between stat structure types and directory types.  */
# define IFTODT(mode)   (((mode) & 0170000) >> 12)
# define DTTOIF(dirtype)        ((dirtype) << 12)

/* This is the data type of directory stream objects.
   The actual structure is opaque to users.  */
typedef struct __dirstream DIR;

#endif
