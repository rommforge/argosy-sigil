// SPDX-License-Identifier: MPL-2.0
#ifndef SIGIL_COMPAT_H
#define SIGIL_COMPAT_H

/* MSVC portability shims. MinGW provides the POSIX names natively. */
#if defined(_MSC_VER)

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

/* MSVC off_t is 32-bit long; force 64-bit so >2GB images seek correctly.
 * <sys/types.h> is included above so its typedef lands before the macro. */
#define off_t long long
#define fseeko _fseeki64
#define ftello _ftelli64

#define strcasecmp _stricmp

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif

#endif /* _MSC_VER */

#endif
