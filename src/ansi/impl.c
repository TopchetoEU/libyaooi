#pragma once

#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ev/conf.h>
#include <ev/errno.h>
#include <ev/filelist.h>
#include <ev/queue.h>
#include <ev/io.h>

#include "./impl.h" // IWYU pragma: export

#include "../utils/lists.h"

#include "../filelist.c"
#include "../time.c"
#include "../queue.c"
#include "../fallback/queue.c" // IWYU pragma: export

#ifdef WIN32
	extern char **_environ;
	#define environ _environ
#elif !defined __USE_GNU
	extern char **environ;
#endif

static char *_evi_ansi_getenvpath(const char *envname, const char *fallback, const char *suffix) {
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

static void evi_ansi_mkfd(ev_filelist_t fl, ev_fd_t res, FILE *f) {
	res->owned = true;
	res->impl.kind = EVI_ANSI_FILE;
	res->impl.file = f;

	evi_dlist_add(fl, fl->fd_head, res);
}
static bool evi_ansi_mkat(ev_filelist_t fl, ev_fd_t res, const char *path) {
	size_t len = strlen(path);

	char *at = malloc(len + 1);
	if (!at) return false;

	memcpy(at, path, len + 1);

	res->owned = true;
	res->impl.kind = EVI_ANSI_AT;
	res->impl.at = at;

	evi_dlist_add(fl, fl->fd_head, res);
	return true;
}
static int evi_ansi_isfd(ev_fd_t fd) {
	return fd->impl.kind == EVI_ANSI_FILE;
}

static ev_code_t evi_ansi_conv_errno(int err, ev_code_t fallback) {
	switch (err) {
		#ifdef EPERM
			case EPERM: return EV_EPERM;
		#endif
		#ifdef ENOENT
			case ENOENT: return EV_ENOENT;
		#endif
		#ifdef ESRCH
			case ESRCH: return EV_ESRCH;
		#endif
		#ifdef EINTR
			case EINTR: return EV_EINTR;
		#endif
		#ifdef EIO
			case EIO: return EV_EIO;
		#endif
		#ifdef ENXIO
			case ENXIO: return EV_ENXIO;
		#endif
		#ifdef E2BIG
			case E2BIG: return EV_E2BIG;
		#endif
		#ifdef ENOEXEC
			case ENOEXEC: return EV_ENOEXEC;
		#endif
		#ifdef EBADF
			case EBADF: return EV_EBADF;
		#endif
		#ifdef ECHILD
			case ECHILD: return EV_ECHILD;
		#endif
		#ifdef EAGAIN
			case EAGAIN: return EV_EAGAIN;
		#endif
		#ifdef ENOMEM
			case ENOMEM: return EV_ENOMEM;
		#endif
		#ifdef EACCES
			case EACCES: return EV_EACCES;
		#endif
		#ifdef EFAULT
			case EFAULT: return EV_EFAULT;
		#endif
		#ifdef ENOTBLK
			case ENOTBLK: return EV_ENOTBLK;
		#endif
		#ifdef EBUSY
			case EBUSY: return EV_EBUSY;
		#endif
		#ifdef EEXIST
			case EEXIST: return EV_EEXIST;
		#endif
		#ifdef EXDEV
			case EXDEV: return EV_EXDEV;
		#endif
		#ifdef ENODEV
			case ENODEV: return EV_ENODEV;
		#endif
		#ifdef ENOTDIR
			case ENOTDIR: return EV_ENOTDIR;
		#endif
		#ifdef EISDIR
			case EISDIR: return EV_EISDIR;
		#endif
		#ifdef EINVAL
			case EINVAL: return EV_EINVAL;
		#endif
		#ifdef ENFILE
			case ENFILE: return EV_ENFILE;
		#endif
		#ifdef EMFILE
			case EMFILE: return EV_EMFILE;
		#endif
		#ifdef ENOTTY
			case ENOTTY: return EV_ENOTTY;
		#endif
		#ifdef ETXTBSY
			case ETXTBSY: return EV_ETXTBSY;
		#endif
		#ifdef EFBIG
			case EFBIG: return EV_EFBIG;
		#endif
		#ifdef ENOSPC
			case ENOSPC: return EV_ENOSPC;
		#endif
		#ifdef ESPIPE
			case ESPIPE: return EV_ESPIPE;
		#endif
		#ifdef EROFS
			case EROFS: return EV_EROFS;
		#endif
		#ifdef EMLINK
			case EMLINK: return EV_EMLINK;
		#endif
		#ifdef EPIPE
			case EPIPE: return EV_EPIPE;
		#endif
		#ifdef EDOM
			case EDOM: return EV_EDOM;
		#endif
		#ifdef ERANGE
			case ERANGE: return EV_ERANGE;
		#endif
		default: return fallback;
	}
}

ev_code_t ev_fd_new(ev_filelist_t fl, ev_fd_t *pres, uint64_t fd, bool owned) {
	ev_fd_t res = malloc(sizeof *res);
	if (!res) return EV_ENOMEM;

	evi_ansi_mkfd(fl, res, (FILE*)(size_t)fd);
	res->owned = owned;
	*pres = res;

	return EV_OK;
}
void ev_fd_close(ev_fd_t fd) {
	if (fd->owned) {
		if (!evi_ansi_isfd(fd)) {
			free(fd->impl.at);
		}
		else {
			// Closing is best-effort
			#ifdef EINTR
				while (fclose(fd->impl.file) < 0) {
					if (errno != EINTR) break;
				}
			#else
				fclose(fd->impl.file);
			#endif
		}
	}

	evi_dlist_del(fl, fd);
	free(fd);
}

ev_code_t ev_read(ev_fd_t fd, char *buff, size_t *pn) {
	if (!evi_ansi_isfd(fd)) return EV_EBADF;

	clearerr(fd->impl.file);
	size_t n = fread(buff, *pn, 1, fd->impl.file);
	if (ferror(fd->impl.file)) return EV_EIO;

	*pn = n;
	return EV_OK;
}
ev_code_t ev_write(ev_fd_t fd, char *buff, size_t *pn) {
	if (!evi_ansi_isfd(fd)) return EV_EBADF;

	clearerr((FILE*)fd);
	size_t n = fwrite(buff, *pn, 1, (FILE*)fd);
	if (ferror(fd->impl.file)) return EV_EIO;

	*pn = n;
	return EV_OK;
}
ev_code_t ev_sync(ev_fd_t fd) {
	if (!evi_ansi_isfd(fd)) return EV_EBADF;
	if (fflush(fd->impl.file) < 0) return EV_EIO;
	return EV_OK;
}
ev_code_t ev_stat(ev_fd_t fd, ev_stat_t *buff) {
	FILE *f;
	bool owned = false;

	if (evi_ansi_isfd(fd)) f = fd->impl.file;
	else {
		f = fopen(fd->impl.at, "r");
		owned = true;
		if (!f) return EV_ENOENT;
	}

	size_t at = ftell(f);
	size_t size = fseek(f, 0, SEEK_END);
	fseek(f, at, SEEK_SET);

	buff->type = EV_STAT_REG;
	buff->mode = 0777;
	buff->gid = 1000;
	buff->uid = 1000;

	buff->atime = (ev_time_t) { 0, 0 };
	buff->ctime = (ev_time_t) { 0, 0 };
	buff->mtime = (ev_time_t) { 0, 0 };

	buff->blksize = 512;
	buff->size = size;

	buff->inode = -1;
	buff->links = 1;

	if (owned) fclose(f);
	return EV_OK;
}

ev_code_t ev_tty_in(ev_filelist_t fl, ev_fd_t *pres) {
	return ev_fd_new(fl, pres, (uint64_t)(size_t)stdin, false);
}
ev_code_t ev_tty_out(ev_filelist_t fl, ev_fd_t *pres) {
	return ev_fd_new(fl, pres, (uint64_t)(size_t)stdout, false);
}
ev_code_t ev_tty_err(ev_filelist_t fl, ev_fd_t *pres) {
	return ev_fd_new(fl, pres, (uint64_t)(size_t)stderr, false);
}
ev_code_t ev_tty_raw(ev_fd_t tty, ev_tty_raw_t *pres) {
	(void)tty, (void)pres;
	return EV_ENOTSUP;
}
ev_code_t ev_tty_rawend(ev_tty_raw_t rawmode) {
	(void)rawmode;
	return EV_ENOTSUP;
}

ev_code_t ev_file_remove(const char *path) {
	if (remove(path) < 0) return evi_ansi_conv_errno(errno, EV_ENOENT);
	return EV_OK;
}
ev_code_t ev_file_symlink(const char *path, const char *target) {
	(void)path, (void)target;
	return EV_ENOTSUP;
}
ev_code_t ev_file_hardlink(const char *path, const char *target) {
	(void)path, (void)target;
	return EV_ENOTSUP;
}
ev_code_t ev_file_readlink(const char *path, char **pres) {
	(void)path, (void)pres;
	return EV_ENOTSUP;
}

ev_code_t ev_file_open(ev_filelist_t fl, ev_fd_t *pres, const char *path, ev_open_flags_t flags, int mode) {
	(void)mode;

	ev_fd_t res = malloc(sizeof *res);
	if (!res) return EV_ENOMEM;

	flags &= ~(EV_OPEN_SHARED | EV_OPEN_DIRECT);

	const char *open_mode;

	switch ((int)flags) {
		case EV_OPEN_STAT: {
			if (!evi_ansi_mkat(fl, res, path)) return EV_ENOMEM;
			*pres = res;
			return EV_OK;
		}
		case EV_OPEN_READ: {
			open_mode = "rb";
			break;
		}
		case EV_OPEN_READ | EV_OPEN_WRITE: {
			open_mode = "rb+";
			break;
		}
		case EV_OPEN_WRITE | EV_OPEN_APPEND | EV_OPEN_CREATE:
		case EV_OPEN_APPEND | EV_OPEN_CREATE: {
			open_mode = "ab";
			break;
		}
		case EV_OPEN_WRITE | EV_OPEN_TRUNC: {
			open_mode = "wb";
			break;
		}
		case EV_OPEN_READ | EV_OPEN_WRITE | EV_OPEN_TRUNC: {
			open_mode = "wb+";
			break;
		}
		default:
			return EV_ENOTSUP;
	}

	FILE *f = fopen(path, open_mode);
	if (!f) return EV_ENOENT;

	evi_ansi_mkfd(fl, res, f);

	*pres = res;
	return EV_OK;
}
ev_code_t ev_file_read(ev_fd_t fd, char *buff, size_t *pn, size_t offset) {
	if (!evi_ansi_isfd(fd)) return EV_EBADF;

	size_t curr = ftell(fd->impl.file);
	if (fseek(fd->impl.file, offset, SEEK_SET) < 0) return EV_ESPIPE;

	size_t n = fread(buff, 1, *pn, fd->impl.file);
	if (ferror(fd->impl.file)) return EV_EIO;

	if (fseek(fd->impl.file, curr, SEEK_SET) < 0) return EV_ESPIPE;

	*pn = n;
	return EV_OK;
}
ev_code_t ev_file_write(ev_fd_t fd, char *buff, size_t *pn, size_t offset) {
	if (!evi_ansi_isfd(fd)) return EV_EBADF;

	size_t curr = ftell(fd->impl.file);
	if (fseek(fd->impl.file, offset, SEEK_SET) < 0) return EV_ESPIPE;

	size_t n = fwrite(buff, 1, *pn, fd->impl.file);
	if (ferror(fd->impl.file)) return EV_EIO;

	if (fseek(fd->impl.file, curr, SEEK_SET) < 0) return EV_ESPIPE;

	*pn = n;
	return EV_OK;
}
ev_code_t ev_file_chmod(ev_fd_t fd, int mode) {
	(void)fd, (void)mode;
	return EV_OK;
}
ev_code_t ev_file_chown(ev_fd_t fd, int uid, int gid) {
	(void)fd, (void)uid, (void)gid;
	return EV_OK;
}

ev_code_t ev_dir_new(const char *path, int mode) {
	(void)path;
	(void)mode;
	return EV_ENOTSUP;
}
ev_code_t ev_dir_open(ev_filelist_t fl, ev_dir_t *pres, const char *path) {
	(void)fl, (void)pres, (void)path;
	return EV_ENOTSUP;
}
ev_code_t ev_dir_next(ev_dir_t dir, char **pname) {
	(void)dir, (void)pname;
	return EV_ENOTSUP;
}
void evs_dir_close(ev_dir_t dir) {
	(void)dir;
}

ev_code_t ev_socket_connect(ev_filelist_t fl, ev_fd_t *pres, ev_proto_t proto, ev_addr_t addr, uint16_t port) {
	(void)fl, (void)pres, (void)proto, (void)addr, (void)port;
	return EV_ENOTSUP;
}
ev_code_t ev_socket_bind(ev_filelist_t fl, ev_fd_t *pres, ev_proto_t proto, ev_addr_t addr, uint16_t port, size_t max_n) {
	(void)fl, (void)pres, (void)proto, (void)addr, (void)port, (void)max_n;
	return EV_ENOTSUP;
}
ev_code_t ev_socket_accept(ev_filelist_t fl, ev_fd_t server, ev_fd_t *pres, ev_addr_t *paddr, uint16_t *pport) {
	(void)fl, (void)pres, (void)server, (void)paddr, (void)pport;
	return EV_ENOTSUP;
}

ev_code_t ev_dns_getaddrinfo(ev_addrinfo_t *pres, const char *name, ev_addrinfo_flags_t flags) {
	(void)pres, (void)name, (void)flags;
	return EV_ENOTSUP;
}

// Equivalent to posix's fork then exec
ev_code_t ev_proc_spawn(
	ev_filelist_t fl, ev_proc_t *pres,
	const char **argv, const char **env, const char *cwd,
	ev_fd_t *pin, ev_fd_t *pout, ev_fd_t *perr
) {
	(void)fl, (void)pres;
	(void)argv, (void)env, (void)cwd;
	(void)pin, (void)pout, (void)perr;
	// TODO: implement with `popen`
	return EV_ENOTSUP;
}
ev_code_t ev_proc_wait(ev_proc_t proc, int *psig, int *pcode) {
	(void)pcode, (void)proc, (void)psig;
	return EV_ENOTSUP;
}
ev_code_t ev_proc_disown(ev_proc_t proc) {
	(void)proc;
	return EV_ENOTSUP;
}

ev_code_t ev_sig_on(ev_signo_t sig) {
	switch (sig) {
		case EV_SIGINT: signal(SIGINT, SIG_IGN); break;
		case EV_SIGABRT: signal(SIGABRT, SIG_IGN); break;
		case EV_SIGTERM: signal(SIGTERM, SIG_IGN); break;

		case EV_SIGBADMEM: signal(SIGSEGV, SIG_IGN); break;
		case EV_SIGBADOP: signal(SIGILL, SIG_IGN); break;

		default: break;
	}

	return EV_OK;
}
ev_code_t ev_sig_off(ev_signo_t sig) {
	switch (sig) {
		case EV_SIGINT: signal(SIGINT, SIG_DFL); break;
		case EV_SIGABRT: signal(SIGABRT, SIG_DFL); break;
		case EV_SIGTERM: signal(SIGTERM, SIG_DFL); break;

		case EV_SIGBADMEM: signal(SIGSEGV, SIG_DFL); break;
		case EV_SIGBADOP: signal(SIGILL, SIG_DFL); break;

		default: break;
	}

	return EV_OK;
}
ev_code_t ev_sig_wait(ev_signo_t *pres) {
	(void)pres;
	// We can't do much more here...
	while (true);
}

ev_code_t ev_getpath(ev_path_type_t type, char **pres) {
	switch (type) {
		case EV_PATH_HOME: {
			*pres = _evi_ansi_getenvpath("HOME", ".", NULL);
			if (!*pres) return EV_ENOMEM;
			return EV_OK;
		}
		case EV_PATH_CACHE: {
			*pres = _evi_ansi_getenvpath("XDG_CACHE_HOME", ".", "/.cache");
			if (!*pres) return EV_ENOMEM;
			return EV_OK;
		}
		case EV_PATH_CONFIG: {
			*pres = _evi_ansi_getenvpath("XDG_CONFIG_HOME", ".", "/.config");
			if (!*pres) return EV_ENOMEM;
			return EV_OK;
		}
		case EV_PATH_DATA: {
			*pres = _evi_ansi_getenvpath("XDG_DATA_HOME", ".", "/.local/share");
			if (!*pres) return EV_ENOMEM;
			return EV_OK;
		}
		case EV_PATH_RUNTIME: {
			*pres = _evi_ansi_getenvpath("XDG_DATA_HOME", "/tmp", NULL);
			if (!*pres) return EV_ENOMEM;
			return EV_OK;
		}
		case EV_PATH_CWD: {
			*pres = _evi_ansi_getenvpath("PWD", ".", NULL);
			if (!*pres) return EV_ENOMEM;
			return EV_OK;
		}
	}

	return EV_EINVAL;
}

ev_code_t ev_env_get(const char *name, char **pres) {
	const char *val = getenv(name);
	if (!val) {
		*pres = NULL;
		return EV_OK;
	}

	char *res = malloc(strlen(val) + 1);
	if (!res) return EV_ENOMEM;

	strcpy(res, val);
	*pres = res;
	return EV_OK;
}
ev_code_t ev_env_set(const char *name, const char *val) {
	if (!val) {
		if (unsetenv(name) < 0) return EV_ENOMEM;
	}
	else {
		if (setenv(name, val, true) < 0) return EV_ENOMEM;
	}

	return EV_OK;
}
ev_code_t ev_nextenv(void **pit, const char **ppair) {
	(void)pit;
	*ppair = NULL;
	return EV_OK;
}

ev_code_t ev_enviter_new(ev_enviter_t *pres) {
	ev_enviter_t res = malloc(sizeof *res);
	if (!res) return EV_ENOMEM;

	res->enviter = environ;

	*pres = res;
	return EV_OK;
}
ev_code_t ev_enviter_next(ev_enviter_t iter, const char **pres) {
	char *pair = *iter->enviter;
	if (pair) iter->enviter++;

	*pres = pair;
	return EV_OK;
}
void ev_enviter_close(ev_enviter_t iter) {
	free(iter);
}

ev_time_t ev_time(ev_clock_t kind) {
	switch (kind) {
		case EV_CLOCK_REALTIME: return (ev_time_t) { .sec = time(NULL), .nsec = 0 };
		case EV_CLOCK_CPUTIME:
		case EV_CLOCK_MONOTIME: {
			clock_t now = clock();
			return (ev_time_t) { .sec = now / CLOCKS_PER_SEC, .nsec = now % CLOCKS_PER_SEC * 1000 };
		}
		default: return (ev_time_t) { 0, 0 };
	}
}
void ev_timesleep(ev_time_t time) {
	// As we have no better option, we will do a spinwait
	clock_t end = time.sec * CLOCKS_PER_SEC + time.nsec / 1000;
	while (clock() < end);
}

#define evq_sig_wait(...) evq_sig_wait(__VA_ARGS__)
ev_code_t (evq_sig_wait)(ev_req_t req, ev_signo_t *pres) {
	(void)pres;
	evi_req_begin(req, NULL);
	return EV_OK;
}
