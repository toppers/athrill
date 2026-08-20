#ifndef ATHRILL_WINDOWS_UNISTD_H
#define ATHRILL_WINDOWS_UNISTD_H

#include <io.h>
#include <process.h>
#include <getopt.h>
#include <BaseTsd.h>

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef SSIZE_T ssize_t;
#endif

#define access _access
#define close _close
#define isatty _isatty
#define lseek _lseek
#define read _read
#define unlink _unlink
#define write _write

#endif /* ATHRILL_WINDOWS_UNISTD_H */
