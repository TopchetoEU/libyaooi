// libyaooi, Copyright (C) 2025-2026 topchetoeu, see LICENSE for full LGPL text

#ifndef YO_ERRNO_H
#define YO_ERRNO_H

#include <yaooi/conf.h>

// These have more or less been ripped from libuv

#define YO_SIGDEF(X) \
	X(OK, 0, "no error occurred") \
	X(EIO, -1, "i/o error") \
	X(ECANCELED, -2, "cancelled") \
	X(ETIMEOUT, -3, "timed out") \
	X(EOF, -4, "end of file") \
	X(EPERM, -5, "operation not permitted") \
	X(EBUSY, -6, "resource busy or locked") \
	X(EAGAIN, -7, "resource temporarily unavailable") \
	X(ENOTSUP, -8, "not supported") \
	X(ENOTIMPL, -9, "not implemented") \
	\
	X(ENOENT, -10, "no such object") \
	X(ENODEV, -11, "no such device") \
	X(EEXIST, -12, "object already exists") \
	X(ENOTFILE, -13, "not a file") \
	X(ENOTDIR, -14, "not a directory") \
	X(ENOTBLK, -15, "not a block device") \
	X(ENOTTTY, -16, "not a tty") \
	X(ENOTSOCK, -17, "not a socket") \
	X(ENOTEMPTY, -18, "directory not empty") \
	X(EISDIR, -19, "illegal operation on a directory") \
	\
	X(EINVAL, -20, "invalid argument") \
	X(EBADFD, -21, "bad file descriptor") \
	X(EBADOP, -22, "invalid operation") \
	X(EXDEV, -23, "cross-device link not permitted") \
	X(ERANGE, -24, "argument or result out of range") \
	X(EFAULT, -25, "bad usage of memory") \
	X(EBADEXE, -26, "corrupted or bad executable") \
	X(EUTF, -27, "illegal utf8 sequence") \
	X(EALREADY, -28, "operation already in progress") \
	\
	X(ENORESOURCE, -30, "resource or limit exhausted") \
	X(ELOOP, -31, "recursive symbolic link") \
	X(EFBIG, -32, "file too large") \
	X(EROFS, -33, "read-only file system") \
	X(ENOMEM, -34, "not enough memory") \
	\
	X(EPROTO, -100, "protocol error") \
	X(EREMOTEIO, -101, "remote I/O error") \
	X(EADDRINUSE, -103, "address already in use") \
	X(EADDRNOTAVAIL, -104, "address not available") \
	X(ENETDOWN, -105, "network is down") \
	X(ECONNABORTED, -106, "software caused connection abort") \
	X(ECONNRESET, -107, "connection reset by peer") \
	X(ECONNREFUSED, -108, "connection refused") \
	X(EHOSTDOWN, -109, "host is down") \
	X(ENOMEDIUM, -110, "no medium found") \
	\
	X(EUNKNOWN, -10000, "unknown error") \

typedef enum {
	#define YO_SIGDEF_ENUM_X(name, code, msg) YO_##name = code,
	YO_SIGDEF(YO_SIGDEF_ENUM_X)
	#undef YO_SIGDEF_ENUM_X
} yo_code_t;

// Converts the error code to a human-readable string
const char *yo_strerr(yo_code_t code);

#endif
