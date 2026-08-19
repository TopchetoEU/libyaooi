// libyaooi, Copyright (C) 2025-2026 topchetoeu, see LICENSE for full LGPL text

#pragma once

#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <yaooi/conf.h>
#include <yaooi/errno.h>
#include <yaooi/queue.h>
#include <yaooi/io.h>

#include "./impl.h" // IWYU pragma: export

#include "../time.c"
#include "../queue.c"
#include "../fallback/queue.c" // IWYU pragma: export

#ifdef WIN32
	extern char **_environ;
	#define environ _environ
#elif !defined __USE_GNU
	extern char **environ;
#endif

static char *_yoi_ansi_getenvpath(const char *envname, const char *fallback, const char *suffix) {
	const char *home = getenv(envname);
	if (!home) home = fallback;

	if (suffix) {
		char *res = malloc(strlen(home) + strlen(suffix) + 1);
		if (!res) return NULL;

		strcpy(res, home);
		strcat(res, suffix);
		return res;
	}
	else {
		char *res = malloc(strlen(home) + 1);
		strcpy(res, home);
		return res;
	}
}

static void yoi_ansi_mkfd(yo_fd_t res, FILE *f) {
	res->owned = true;
	res->kind = YOI_ANSI_FILE;
	res->file = f;
}
static bool yoi_ansi_mkat(yo_fd_t res, const char *path) {
	size_t len = strlen(path);

	char *at = malloc(len + 1);
	if (!at) return false;

	memcpy(at, path, len + 1);

	res->owned = true;
	res->kind = YOI_ANSI_AT;
	res->at = at;
	return true;
}
static int yoi_ansi_isfd(yo_fd_t fd) {
	return fd->kind == YOI_ANSI_FILE;
}

static yo_code_t yoi_ansi_conv_errno(int err, yo_code_t fallback) {
	switch (err) {
		#ifdef EPERM
			case EPERM: return YO_EPERM;
		#endif
		#ifdef ENOENT
			case ENOENT: return YO_ENOENT;
		#endif
		#ifdef ESRCH
			case ESRCH: return YO_ESRCH;
		#endif
		#ifdef EINTR
			case EINTR: return YO_EINTR;
		#endif
		#ifdef EIO
			case EIO: return YO_EIO;
		#endif
		#ifdef ENXIO
			case ENXIO: return YO_ENXIO;
		#endif
		#ifdef E2BIG
			case E2BIG: return YO_E2BIG;
		#endif
		#ifdef ENOEXEC
			case ENOEXEC: return YO_ENOEXEC;
		#endif
		#ifdef EBADF
			case EBADF: return YO_EBADF;
		#endif
		#ifdef ECHILD
			case ECHILD: return YO_ECHILD;
		#endif
		#ifdef EAGAIN
			case EAGAIN: return YO_EAGAIN;
		#endif
		#ifdef ENOMEM
			case ENOMEM: return YO_ENOMEM;
		#endif
		#ifdef EACCES
			case EACCES: return YO_EACCES;
		#endif
		#ifdef EFAULT
			case EFAULT: return YO_EFAULT;
		#endif
		#ifdef ENOTBLK
			case ENOTBLK: return YO_ENOTBLK;
		#endif
		#ifdef EBUSY
			case EBUSY: return YO_EBUSY;
		#endif
		#ifdef EEXIST
			case EEXIST: return YO_EEXIST;
		#endif
		#ifdef EXDEV
			case EXDEV: return YO_EXDEV;
		#endif
		#ifdef ENODEV
			case ENODEV: return YO_ENODEV;
		#endif
		#ifdef ENOTDIR
			case ENOTDIR: return YO_ENOTDIR;
		#endif
		#ifdef EISDIR
			case EISDIR: return YO_EISDIR;
		#endif
		#ifdef EINVAL
			case EINVAL: return YO_EINVAL;
		#endif
		#ifdef ENFILE
			case ENFILE: return YO_ENFILE;
		#endif
		#ifdef EMFILE
			case EMFILE: return YO_EMFILE;
		#endif
		#ifdef ENOTTY
			case ENOTTY: return YO_ENOTTY;
		#endif
		#ifdef ETXTBSY
			case ETXTBSY: return YO_ETXTBSY;
		#endif
		#ifdef EFBIG
			case EFBIG: return YO_EFBIG;
		#endif
		#ifdef ENOSPC
			case ENOSPC: return YO_ENOSPC;
		#endif
		#ifdef ESPIPE
			case ESPIPE: return YO_ESPIPE;
		#endif
		#ifdef EROFS
			case EROFS: return YO_EROFS;
		#endif
		#ifdef EMLINK
			case EMLINK: return YO_EMLINK;
		#endif
		#ifdef EPIPE
			case EPIPE: return YO_EPIPE;
		#endif
		#ifdef EDOM
			case EDOM: return YO_EDOM;
		#endif
		#ifdef ERANGE
			case ERANGE: return YO_ERANGE;
		#endif
		default: return fallback;
	}
}

yo_code_t yo_fd_new(yo_fd_t *pres, uint64_t fd, bool owned) {
	yo_fd_t res = malloc(sizeof *res);
	if (!res) return YO_ENOMEM;

	yoi_ansi_mkfd(res, (FILE*)(size_t)fd);
	res->owned = owned;
	*pres = res;

	return YO_OK;
}
void yo_fd_close(yo_fd_t fd) {
	if (fd->owned) {
		if (!yoi_ansi_isfd(fd)) {
			free(fd->at);
		}
		else {
			// Closing is best-effort
			#ifdef EINTR
				while (fclose(fd->file) < 0) {
					if (errno != EINTR) break;
				}
			#else
				fclose(fd->file);
			#endif
		}
	}

	free(fd);
}

yo_code_t yo_read(yo_fd_t fd, char *buff, size_t *pn) {
	if (!yoi_ansi_isfd(fd)) return YO_EBADF;

	clearerr(fd->file);
	size_t n = fread(buff, *pn, 1, fd->file);
	if (ferror(fd->file)) return YO_EIO;

	*pn = n;
	return YO_OK;
}
yo_code_t yo_write(yo_fd_t fd, char *buff, size_t *pn) {
	if (!yoi_ansi_isfd(fd)) return YO_EBADF;

	clearerr((FILE*)fd);
	size_t n = fwrite(buff, *pn, 1, (FILE*)fd);
	if (ferror(fd->file)) return YO_EIO;

	*pn = n;
	return YO_OK;
}
yo_code_t yo_sync(yo_fd_t fd) {
	if (!yoi_ansi_isfd(fd)) return YO_EBADF;
	if (fflush(fd->file) < 0) return YO_EIO;
	return YO_OK;
}
yo_code_t yo_stat(yo_fd_t fd, yo_stat_t *buff) {
	FILE *f;
	bool owned = false;

	if (yoi_ansi_isfd(fd)) f = fd->file;
	else {
		f = fopen(fd->at, "r");
		owned = true;
		if (!f) return YO_ENOENT;
	}

	size_t at = ftell(f);
	size_t size = fseek(f, 0, SEEK_END);
	fseek(f, at, SEEK_SET);

	buff->type = YO_STAT_REG;
	buff->mode = 0777;
	buff->gid = 1000;
	buff->uid = 1000;

	buff->atime = (yo_time_t) { 0, 0 };
	buff->ctime = (yo_time_t) { 0, 0 };
	buff->mtime = (yo_time_t) { 0, 0 };

	buff->blksize = 512;
	buff->size = size;

	buff->inode = -1;
	buff->links = 1;

	if (owned) fclose(f);
	return YO_OK;
}

yo_code_t yo_tty_in(yo_fd_t *pres) {
	return yo_fd_new(pres, (uint64_t)(size_t)stdin, false);
}
yo_code_t yo_tty_out(yo_fd_t *pres) {
	return yo_fd_new(pres, (uint64_t)(size_t)stdout, false);
}
yo_code_t yo_tty_err(yo_fd_t *pres) {
	return yo_fd_new(pres, (uint64_t)(size_t)stderr, false);
}
yo_code_t yo_tty_raw(yo_fd_t tty, yo_tty_raw_t *pres) {
	(void)tty, (void)pres;
	return YO_ENOTSUP;
}
yo_code_t yo_tty_rawend(yo_tty_raw_t rawmode) {
	(void)rawmode;
	return YO_ENOTSUP;
}

yo_code_t yo_file_remove(const char *path) {
	if (remove(path) < 0) return yoi_ansi_conv_errno(errno, YO_ENOENT);
	return YO_OK;
}
yo_code_t yo_file_symlink(const char *path, const char *target) {
	(void)path, (void)target;
	return YO_ENOTSUP;
}
yo_code_t yo_file_hardlink(const char *path, const char *target) {
	(void)path, (void)target;
	return YO_ENOTSUP;
}
yo_code_t yo_file_readlink(const char *path, char **pres) {
	(void)path, (void)pres;
	return YO_ENOTSUP;
}

yo_code_t yo_file_open(yo_fd_t *pres, const char *path, yo_open_flags_t flags, int mode) {
	(void)mode;

	yo_fd_t res = malloc(sizeof *res);
	if (!res) return YO_ENOMEM;

	flags &= ~(YO_OPEN_SHARED | YO_OPEN_DIRECT);

	const char *open_mode;

	switch ((int)flags) {
		case YO_OPEN_STAT: {
			if (!yoi_ansi_mkat(res, path)) return YO_ENOMEM;
			*pres = res;
			return YO_OK;
		}
		case YO_OPEN_READ: {
			open_mode = "rb";
			break;
		}
		case YO_OPEN_READ | YO_OPEN_WRITE: {
			open_mode = "rb+";
			break;
		}
		case YO_OPEN_WRITE | YO_OPEN_APPEND | YO_OPEN_CREATE:
		case YO_OPEN_APPEND | YO_OPEN_CREATE: {
			open_mode = "ab";
			break;
		}
		case YO_OPEN_WRITE | YO_OPEN_TRUNC: {
			open_mode = "wb";
			break;
		}
		case YO_OPEN_READ | YO_OPEN_WRITE | YO_OPEN_TRUNC: {
			open_mode = "wb+";
			break;
		}
		default:
			return YO_ENOTSUP;
	}

	FILE *f = fopen(path, open_mode);
	if (!f) return YO_ENOENT;

	yoi_ansi_mkfd(res, f);

	*pres = res;
	return YO_OK;
}
yo_code_t yo_file_read(yo_fd_t fd, char *buff, size_t *pn, size_t offset) {
	if (!yoi_ansi_isfd(fd)) return YO_EBADF;

	size_t curr = ftell(fd->file);
	if (fseek(fd->file, offset, SEEK_SET) < 0) return YO_ESPIPE;

	size_t n = fread(buff, 1, *pn, fd->file);
	if (ferror(fd->file)) return YO_EIO;

	if (fseek(fd->file, curr, SEEK_SET) < 0) return YO_ESPIPE;

	*pn = n;
	return YO_OK;
}
yo_code_t yo_file_write(yo_fd_t fd, char *buff, size_t *pn, size_t offset) {
	if (!yoi_ansi_isfd(fd)) return YO_EBADF;

	size_t curr = ftell(fd->file);
	if (fseek(fd->file, offset, SEEK_SET) < 0) return YO_ESPIPE;

	size_t n = fwrite(buff, 1, *pn, fd->file);
	if (ferror(fd->file)) return YO_EIO;

	if (fseek(fd->file, curr, SEEK_SET) < 0) return YO_ESPIPE;

	*pn = n;
	return YO_OK;
}
yo_code_t yo_file_chmod(yo_fd_t fd, int mode) {
	(void)fd, (void)mode;
	return YO_OK;
}
yo_code_t yo_file_chown(yo_fd_t fd, int uid, int gid) {
	(void)fd, (void)uid, (void)gid;
	return YO_OK;
}

yo_code_t yo_dir_new(const char *path, int mode) {
	(void)path;
	(void)mode;
	return YO_ENOTSUP;
}
yo_code_t yo_dir_open(yo_dir_t *pres, const char *path) {
	(void)pres, (void)path;
	return YO_ENOTSUP;
}
yo_code_t yo_dir_next(yo_dir_t dir, char **pname) {
	(void)dir, (void)pname;
	return YO_ENOTSUP;
}
void evs_dir_close(yo_dir_t dir) {
	(void)dir;
}

yo_code_t yo_socket_connect(yo_fd_t *pres, yo_proto_t proto, yo_addr_t addr, uint16_t port) {
	(void)pres, (void)proto, (void)addr, (void)port;
	return YO_ENOTSUP;
}
yo_code_t yo_socket_bind(yo_fd_t *pres, yo_proto_t proto, yo_addr_t addr, uint16_t port, size_t max_n) {
	(void)pres, (void)proto, (void)addr, (void)port, (void)max_n;
	return YO_ENOTSUP;
}
yo_code_t yo_socket_accept(yo_fd_t server, yo_fd_t *pres, yo_addr_t *paddr, uint16_t *pport) {
	(void)pres, (void)server, (void)paddr, (void)pport;
	return YO_ENOTSUP;
}

yo_code_t yo_dns_getaddrinfo(yo_addrinfo_t *pres, const char *name, yo_addrinfo_flags_t flags) {
	(void)pres, (void)name, (void)flags;
	return YO_ENOTSUP;
}

// Equivalent to posix's fork then exec
yo_code_t yo_proc_spawn(
	yo_proc_t *pres, yo_spawn_flags_t flags,
	const char **argv, const char **env, const char *cwd,
	yo_fd_t *pin, yo_fd_t *pout, yo_fd_t *perr
) {
	(void)pres, (void)flags;
	(void)argv, (void)env, (void)cwd;
	(void)pin, (void)pout, (void)perr;
	// TODO: implement with `popen`
	return YO_ENOTSUP;
}
yo_code_t yo_proc_wait(yo_proc_t proc, int *psig, int *pcode) {
	(void)pcode, (void)proc, (void)psig;
	return YO_ENOTSUP;
}
yo_code_t yo_proc_disown(yo_proc_t proc) {
	(void)proc;
	return YO_ENOTSUP;
}

yo_code_t yo_sig_on(yo_signo_t sig) {
	switch (sig) {
		case YO_SIGINT: signal(SIGINT, SIG_IGN); break;
		case YO_SIGABRT: signal(SIGABRT, SIG_IGN); break;
		case YO_SIGTERM: signal(SIGTERM, SIG_IGN); break;

		case YO_SIGBADMEM: signal(SIGSEGV, SIG_IGN); break;
		case YO_SIGBADOP: signal(SIGILL, SIG_IGN); break;

		default: break;
	}

	return YO_OK;
}
yo_code_t yo_sig_off(yo_signo_t sig) {
	switch (sig) {
		case YO_SIGINT: signal(SIGINT, SIG_DFL); break;
		case YO_SIGABRT: signal(SIGABRT, SIG_DFL); break;
		case YO_SIGTERM: signal(SIGTERM, SIG_DFL); break;

		case YO_SIGBADMEM: signal(SIGSEGV, SIG_DFL); break;
		case YO_SIGBADOP: signal(SIGILL, SIG_DFL); break;

		default: break;
	}

	return YO_OK;
}
yo_code_t yo_sig_wait(yo_signo_t *pres) {
	(void)pres;
	// We can't do much more here...
	while (true);
}

yo_code_t yo_getpath(yo_path_type_t type, char **pres) {
	switch (type) {
		case YO_PATH_HOME: {
			*pres = _yoi_ansi_getenvpath("HOME", ".", NULL);
			if (!*pres) return YO_ENOMEM;
			return YO_OK;
		}
		case YO_PATH_CACHE: {
			*pres = _yoi_ansi_getenvpath("XDG_CACHE_HOME", ".", "/.cache");
			if (!*pres) return YO_ENOMEM;
			return YO_OK;
		}
		case YO_PATH_CONFIG: {
			*pres = _yoi_ansi_getenvpath("XDG_CONFIG_HOME", ".", "/.config");
			if (!*pres) return YO_ENOMEM;
			return YO_OK;
		}
		case YO_PATH_DATA: {
			*pres = _yoi_ansi_getenvpath("XDG_DATA_HOME", ".", "/.local/share");
			if (!*pres) return YO_ENOMEM;
			return YO_OK;
		}
		case YO_PATH_RUNTIME: {
			*pres = _yoi_ansi_getenvpath("XDG_DATA_HOME", "/tmp", NULL);
			if (!*pres) return YO_ENOMEM;
			return YO_OK;
		}
		case YO_PATH_CWD: {
			*pres = _yoi_ansi_getenvpath("PWD", ".", NULL);
			if (!*pres) return YO_ENOMEM;
			return YO_OK;
		}
	}

	return YO_EINVAL;
}

yo_code_t yo_env_get(const char *name, char **pres) {
	const char *val = getenv(name);
	if (!val) {
		*pres = NULL;
		return YO_OK;
	}

	char *res = malloc(strlen(val) + 1);
	if (!res) return YO_ENOMEM;

	strcpy(res, val);
	*pres = res;
	return YO_OK;
}
yo_code_t yo_env_set(const char *name, const char *val) {
	if (!val) {
		if (unsetenv(name) < 0) return YO_ENOMEM;
	}
	else {
		if (setenv(name, val, true) < 0) return YO_ENOMEM;
	}

	return YO_OK;
}
yo_code_t yo_nextenv(void **pit, const char **ppair) {
	(void)pit;
	*ppair = NULL;
	return YO_OK;
}

yo_enviter_t yo_enviter_new() {
	yo_enviter_t res = malloc(sizeof *res);
	if (!res) return NULL;

	res->enviter = environ;

	return res;
}
yo_code_t yo_enviter_next(yo_enviter_t iter, const char **pres) {
	char *pair = *iter->enviter;
	if (pair) iter->enviter++;

	*pres = pair;
	return YO_OK;
}
void yo_enviter_close(yo_enviter_t iter) {
	free(iter);
}

yo_time_t yo_time(yo_clock_t kind) {
	switch (kind) {
		case YO_CLOCK_REALTIME: return (yo_time_t) { .sec = time(NULL), .nsec = 0 };
		case YO_CLOCK_CPUTIME:
		case YO_CLOCK_MONOTIME: {
			clock_t now = clock();
			return (yo_time_t) { .sec = now / CLOCKS_PER_SEC, .nsec = now % CLOCKS_PER_SEC * 1000 };
		}
		default: return (yo_time_t) { 0, 0 };
	}
}
void yo_timesleep(yo_time_t time) {
	// As we have no better option, we will do a spinwait
	clock_t end = time.sec * CLOCKS_PER_SEC + time.nsec / 1000;
	while (clock() < end);
}

#define yoa_sig_wait(...) yoa_sig_wait(__VA_ARGS__)
yo_code_t (yoa_sig_wait)(yo_req_t req, yo_signo_t *pres) {
	(void)pres;
	yoi_req_begin(req, yoi_req_cancel_noop_cb);
	return YO_OK;
}
