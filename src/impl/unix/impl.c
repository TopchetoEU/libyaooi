#pragma once

#define _GNU_SOURCE
#include <unistd.h>
#include <netdb.h>
#include <pwd.h>
#include <dirent.h>
#include <limits.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <ev/filelist.h>
#include <ev/io.h>
#include <ev/conf.h>
#include <ev/errno.h>
#include <ev/signo.h>

#ifdef EV_USE_URING
	#include <sys/signalfd.h>
#endif

#include "../../utils/lists.h"
#include "../../utils/multithread.h"

#include "./impl.h" // IWYU pragma: export

#include "../../core/time.c"
#include "../../core/filelist.c"
#include "./async.c" // IWYU pragma: export

#ifndef __USE_GNU
	extern char **environ;
#endif

static bool _core_sig_init = false;
static ev_mutex_t _core_sig_mut;
static size_t _core_sig_counts[EV_SIGUSR2 + 1];
static sigset_t _core_sig_set;

static bool evi_unix_isfd(ev_fd_t fd) {
	#ifndef EV_USE_LINUX
		return !fd->is_at;
	#else
		(void)fd;
		return true;
	#endif
}
static void evi_unix_mkfd(ev_filelist_t fl, ev_fd_t res, int fd) {
	res->impl.fd = fd;
	#ifndef EV_USE_LINUX
		res->impl.is_fd = true;
	#endif

	evi_dlist_add(fl, fl->fd_head, res);
}
#ifndef EV_USE_LINUX
static void evi_unix_mkat(ev_filelist_t fl, ev_fd_t res, const char *at) {
	res->impl.at = at;
	res->impl.is_fd = false;

	evi_dlist_add(fl, fl->fd_head, res);
}
#endif

static int evi_unix_conv_open_flags(ev_open_flags_t flags) {
	int res = 0;

	if (flags & EV_OPEN_STAT) {
		#ifdef EV_USE_LINUX
			res |= O_PATH;
		#endif
	}
	else {
		if (flags & EV_OPEN_APPEND) {
			flags |= EV_OPEN_WRITE;
			res |= O_APPEND;
		}

		if (flags & EV_OPEN_WRITE) {
			if (flags & EV_OPEN_READ) {
				res |= O_RDWR;
			}
			else {
				res |= O_WRONLY;
			}
		}
		else if (flags & EV_OPEN_READ) {
			res |= O_RDONLY;
		}
	}

	if (flags & EV_OPEN_CREATE) res |= O_CREAT;
	if (flags & EV_OPEN_TRUNC) res |= O_TRUNC;
	if (flags & EV_OPEN_DIRECT) res |= O_SYNC;
	if (flags & EV_OPEN_NOFOLLOW) res |= O_NOFOLLOW;
	if (!(flags & EV_OPEN_SHARED)) res |= O_CLOEXEC;

	return res;
}
static void evi_unix_conv_stat_mode(int mode, ev_stat_t *dst) {
	switch (mode & S_IFMT) {
		case S_IFREG: dst->type = EV_STAT_REG; break;
		case S_IFDIR: dst->type = EV_STAT_DIR; break;
		case S_IFLNK: dst->type = EV_STAT_LINK; break;
		case S_IFSOCK: dst->type = EV_STAT_SOCK; break;
		case S_IFIFO: dst->type = EV_STAT_FIFO; break;
		case S_IFCHR: dst->type = EV_STAT_CHAR; break;
		case S_IFBLK: dst->type = EV_STAT_BLK; break;
		default: dst->type = -1; break;
	}

	dst->mode = mode & ~S_IFMT;
}
static void evi_unix_conv_stat(ev_stat_t *dst, struct stat *src) {
	evi_unix_conv_stat_mode(src->st_mode, dst);

	dst->mode = src->st_mode & ~S_IFMT;
	dst->uid = src->st_uid;
	dst->gid = src->st_gid;
	dst->atime = (ev_time_t) { .sec = src->st_atim.tv_sec, .nsec = src->st_atim.tv_nsec };
	dst->ctime = (ev_time_t) { .sec = src->st_ctim.tv_sec, .nsec = src->st_ctim.tv_nsec };
	dst->mtime = (ev_time_t) { .sec = src->st_mtim.tv_sec, .nsec = src->st_mtim.tv_nsec };
	dst->size = src->st_size;
	dst->inode = src->st_ino;
	dst->links = src->st_nlink;
	dst->blksize = src->st_blksize;
}

static int evi_unix_conv_signal(int sig) {
	switch (sig) {
		case SIGHUP: return EV_SIGTLOST;
		case SIGINT: return EV_SIGINT;
		case SIGQUIT: return EV_SIGQUIT;
		case SIGILL: return EV_SIGBADOP;
		case SIGABRT: return EV_SIGABRT;
		case SIGBUS: return EV_SIGBADMEM;
		case SIGFPE: return EV_SIGBADOP;
		case SIGUSR1: return EV_SIGUSR1;
		case SIGSEGV: return EV_SIGBADMEM;
		case SIGUSR2: return EV_SIGUSR2;
		case SIGPIPE: return EV_SIGBADPIPE;
		case SIGTERM: return EV_SIGTERM;
		case SIGSTKFLT: return EV_SIGBADMEM;
		case SIGWINCH: return EV_SIGTSIZE;
		case SIGSYS: return EV_SIGBADOP;
	}
	return -1;
}
static ev_code_t evi_unix_conv_errno(int unixerr) {
	switch (unixerr) {
		case EPERM: return EV_EPERM;
		case ENOENT: return EV_ENOENT;
		case ESRCH: return EV_ESRCH;
		case EINTR: return EV_EINTR;
		case EIO: return EV_EIO;
		case ENXIO: return EV_ENXIO;
		case E2BIG: return EV_E2BIG;
		case ENOEXEC: return EV_ENOEXEC;
		case EBADF: return EV_EBADF;
		case ECHILD: return EV_ECHILD;
		case EAGAIN: return EV_EAGAIN;
		case ENOMEM: return EV_ENOMEM;
		case EACCES: return EV_EACCES;
		case EFAULT: return EV_EFAULT;
		case EBUSY: return EV_EBUSY;
		case EEXIST: return EV_EEXIST;
		case EXDEV: return EV_EXDEV;
		case ENODEV: return EV_ENODEV;
		case ENOTDIR: return EV_ENOTDIR;
		case EISDIR: return EV_EISDIR;
		case EINVAL: return EV_EINVAL;
		case ENFILE: return EV_ENFILE;
		case EMFILE: return EV_EMFILE;
		case ENOTTY: return EV_ENOTTY;
		case ETXTBSY: return EV_ETXTBSY;
		case EFBIG: return EV_EFBIG;
		case ENOSPC: return EV_ENOSPC;
		case ESPIPE: return EV_ESPIPE;
		case EROFS: return EV_EROFS;
		case EMLINK: return EV_EMLINK;
		case EPIPE: return EV_EPIPE;
		case ERANGE: return EV_ERANGE;
		case EDEADLK: return EV_EDEADLK;
		case ENAMETOOLONG: return EV_ENAMETOOLONG;
		case ENOLCK: return EV_ENOLCK;
		case ENOSYS: return EV_ENOSYS;
		case ENOTEMPTY: return EV_ENOTEMPTY;
		case ELOOP: return EV_ELOOP;
		case EUNATCH: return EV_EUNATCH;
		case ENODATA: return EV_ENODATA;
		case ENONET: return EV_ENONET;
		case ECOMM: return EV_ECOMM;
		case EPROTO: return EV_EPROTO;
		case EOVERFLOW: return EV_EOVERFLOW;
		case ENOTUNIQ: return EV_ENOTUNIQ;
		case ELIBBAD: return EV_ELIBBAD;
		case EILSEQ: return EV_EILSEQ;
		case ENOTSOCK: return EV_ENOTSOCK;
		case EDESTADDRREQ: return EV_EDESTADDRREQ;
		case EMSGSIZE: return EV_EMSGSIZE;
		case EPROTOTYPE: return EV_EPROTOTYPE;
		case ENOPROTOOPT: return EV_ENOPROTOOPT;
		case EPROTONOSUPPORT: return EV_EPROTONOSUPPORT;
		case ESOCKTNOSUPPORT: return EV_ESOCKTNOSUPPORT;
		case ENOTSUP: return EV_ENOTSUP;
		case EPFNOSUPPORT: return EV_EPFNOSUPPORT;
		case EAFNOSUPPORT: return EV_EAFNOSUPPORT;
		case EADDRINUSE: return EV_EADDRINUSE;
		case EADDRNOTAVAIL: return EV_EADDRNOTAVAIL;
		case ENETDOWN: return EV_ENETDOWN;
		case ENETUNREACH: return EV_ENETUNREACH;
		case ECONNABORTED: return EV_ECONNABORTED;
		case ECONNRESET: return EV_ECONNRESET;
		case ENOBUFS: return EV_ENOBUFS;
		case EISCONN: return EV_EISCONN;
		case ENOTCONN: return EV_ENOTCONN;
		case ESHUTDOWN: return EV_ESHUTDOWN;
		case ETIMEDOUT: return EV_ETIMEDOUT;
		case ECONNREFUSED: return EV_ECONNREFUSED;
		case EHOSTDOWN: return EV_EHOSTDOWN;
		case EHOSTUNREACH: return EV_EHOSTUNREACH;
		case EALREADY: return EV_EALREADY;
		case EREMOTEIO: return EV_EREMOTEIO;
		case ENOMEDIUM: return EV_ENOMEDIUM;
		case ECANCELED: return EV_ECANCELED;
		case -1: return EV_EUNKNOWN;
		default: return EV_EUNKNOWN;
	}
}
static ev_code_t evi_unix_conv_aierr(int aierr) {
	switch (aierr) {
		case EAI_BADFLAGS: return EV_EAI_BADFLAGS;
		case EAI_NONAME: return EV_EAI_NONAME;
		case EAI_AGAIN: return EV_EAI_AGAIN;
		case EAI_FAIL: return EV_EAI_FAIL;
		case EAI_FAMILY: return EV_EAI_FAMILY;
		case EAI_SOCKTYPE: return EV_EAI_SOCKTYPE;
		case EAI_SERVICE: return EV_EAI_SERVICE;
		case EAI_MEMORY: return EV_EAI_MEMORY;
		case EAI_OVERFLOW: return EV_EAI_OVERFLOW;
		#ifdef EV_USE_LINUX
			case EAI_NODATA: return EV_EAI_NODATA;
			case EAI_ADDRFAMILY: return EV_EAI_ADDRFAMILY;
			case EAI_CANCELED: return EV_EAI_CANCELED;
		#endif

		default: return EV_EUNKNOWN;
	}
}

static int evi_unix_conv_addr(ev_addr_t addr, uint16_t port, struct sockaddr_storage *pres) {
	if (addr.type == EV_ADDR_IPV4) {
		struct sockaddr_in res;
		res.sin_family = AF_INET;
		res.sin_port = htons(port);
		// TODO: check if order is correct
		memcpy(&res.sin_addr, addr.v4, sizeof res.sin_addr);
		memcpy(pres, &res, sizeof res);
		return sizeof res;
	}
	else {
		struct sockaddr_in6 res;
		res.sin6_family = AF_INET6;
		res.sin6_port = htons(port);
		for (size_t i = 0; i < 8; i++) {
			uint16_t netord = htons(addr.v6[i]);
			memcpy((void*)&res.sin6_addr + i * 2, &netord, 2);
		}
		memcpy(pres, &res, sizeof res);
		return sizeof res;
	}
}
static void evi_unix_conv_sockaddr(struct sockaddr_storage *sockaddr, ev_addr_t *pres, uint16_t *pport) {
	if (sockaddr->ss_family == AF_INET) {
		struct sockaddr_in *sockaddr_in = (void*)sockaddr;

		*pport = ntohs(sockaddr_in->sin_port);
		pres->type = EV_ADDR_IPV4;
		memcpy(pres->v4, &sockaddr_in->sin_addr, sizeof sockaddr_in->sin_addr);
	}
	else {
		struct sockaddr_in6 *sockaddr_in6 = (void*)sockaddr;

		*pport = ntohs(sockaddr_in6->sin6_port);
		pres->type = EV_ADDR_IPV6;
		memcpy(pres->v6, &sockaddr_in6->sin6_addr, sizeof sockaddr_in6->sin6_addr);

		for (size_t i = 0; i < 8; i++) {
			pres->v6[i] = ntohs(pres->v6[i]);
		}
	}
}

static char *evi_generic_getenvpath(const char *suffix) {
	struct passwd resbuf[1];
	struct passwd *ppwd;
	char *buff = malloc(PATH_MAX);
	if (!buff) return NULL;

	size_t buffn = PATH_MAX;

	while (true) {
		getpwuid_r(getuid(), resbuf, buff, buffn, &ppwd);
		if (ppwd) break;
		if (errno == ERANGE) {
			buffn *= 2;
			free(buff);
			buff = malloc(buffn);
			if (!buff) return NULL;
		}
		else {
			free(buff);
			return NULL;
		}
	}

	if (suffix) {
		char *res = malloc(strlen(ppwd->pw_dir) + strlen(suffix) + 1);
		if (!res) return NULL;

		strcpy(res, ppwd->pw_dir);
		strcat(res, suffix);
		free(buff);
		return res;
	}
	else {
		char *res = malloc(strlen(ppwd->pw_dir) + 1);
		strcpy(res, ppwd->pw_dir);
		free(buff);
		return res;
	}
}
static char *evi_unix_getpath(const char *envname, const char *suffix) {
	const char *env = getenv(envname);
	if (env && *env) {
		char *res = malloc(strlen(env) + 1);
		if (!res) return NULL;

		strcpy(res, env);
		return res;
	}

	return evi_generic_getenvpath(suffix);
}

// Equivalent to socket(). The created socket is stored in `socket`
static int evi_unix_socket_new(ev_proto_t proto, ev_addr_type_t addr) {
	return socket(
		addr == EV_ADDR_IPV4 ? AF_INET : AF_INET6,
		proto == EV_PROTO_UDP ? SOCK_DGRAM : SOCK_STREAM,
		proto == EV_PROTO_UDP ? IPPROTO_UDP : IPPROTO_TCP
	);
}

static int evi_unix_mkstd(bool in, int *pparent, int *pchild, ev_fd_t *pres) {
	ev_fd_t res = malloc(sizeof *res);
	if (!res) return -1;

	int pipe_fd[2];
	if (pipe(pipe_fd) < 0) {
		free(res);
		return -1;
	}

	if (in) {
		*pparent = pipe_fd[1];
		*pchild = pipe_fd[0];
	}
	else {
		*pparent = pipe_fd[0];
		*pchild = pipe_fd[1];
	}

	*pres = res;

	return 0;
}

ev_code_t ev_fd_new(ev_filelist_t fl, ev_fd_t *pres, uint64_t fd) {
	ev_fd_t res = malloc(sizeof *res);
	if (!res) return EV_ENOMEM;

	evi_unix_mkfd(fl, res, fd);
	*pres = res;

	return EV_OK;
}
void ev_fd_close(ev_fd_t fd) {
	#ifndef EV_USE_LINUX
	if (fd->impl.is_at) {
		free(fd->impl.at);
	}
	#endif

	while (close(fd->impl.fd) < 0) {
		if (errno != EINTR) return;
	}

	evi_dlist_del(fl, fd);
	free(fd);
}

ev_code_t ev_read(ev_fd_t fd, char *buff, size_t *pn) {
	if (!evi_unix_isfd(fd)) return EV_EBADF;

	ssize_t n = read(fd->impl.fd, buff, *pn);
	if (n < 0) return evi_unix_conv_errno(errno);

	*pn = n;
	return EV_OK;
}
ev_code_t ev_write(ev_fd_t fd, char *buff, size_t *pn) {
	if (!evi_unix_isfd(fd)) return EV_EBADF;

	ssize_t n = write(fd->impl.fd, buff, *pn);
	if (n < 0) return evi_unix_conv_errno(errno);

	*pn = n;
	return EV_OK;
}
ev_code_t ev_sync(ev_fd_t fd) {
	if (!evi_unix_isfd(fd)) return EV_EBADF;
	return evi_unix_conv_errno(fsync(fd->impl.fd));
}
ev_code_t ev_stat(ev_fd_t fd, ev_stat_t *buff) {
	struct stat res;

	if (evi_unix_isfd(fd)) {
		if (fstat(fd->impl.fd, &res) < 0) return evi_unix_conv_errno(errno);
	}
	#ifndef EV_USE_LINUX
	else (fd->core.kind == EVI_UNIX_AT) {
		// TODO: respect NOFOLLOW
		if (lstat(fd->core.fd, &res) < 0) return evi_unix_conv_errno(errno);
	}
	#endif

	evi_unix_conv_stat(buff, &res);
	return EV_OK;
}

ev_code_t ev_tty_in(ev_filelist_t fl, ev_fd_t *pres) {
	return ev_fd_new(fl, pres, STDIN_FILENO);
}
ev_code_t ev_tty_out(ev_filelist_t fl, ev_fd_t *pres) {
	return ev_fd_new(fl, pres, STDOUT_FILENO);
}
ev_code_t ev_tty_err(ev_filelist_t fl, ev_fd_t *pres) {
	return ev_fd_new(fl, pres, STDERR_FILENO);
}
ev_code_t ev_tty_raw(ev_fd_t tty, ev_tty_raw_t *pres) {
	if (!evi_unix_isfd(tty)) return EV_EBADF;

	ev_tty_raw_t res = malloc(sizeof *res);
	if (!res) return EV_ENOMEM;

	res->fd = tty->impl.fd;

	if (tcgetattr(res->fd, &res->prev_mode) < 0) {
		free(res);
		return evi_unix_conv_errno(errno);
	}
	struct termios raw = res->prev_mode;

	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= CS8;
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

	raw.c_cc[VMIN]  = 1;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(res->fd, TCSAFLUSH, &raw) < 0) {
		free(res);
		return evi_unix_conv_errno(errno);
	}

	*pres = res;
	return EV_OK;
}
ev_code_t ev_tty_rawend(ev_tty_raw_t rawmode) {
	if (tcsetattr(rawmode->fd, TCSAFLUSH, &rawmode->prev_mode) < 0) return evi_unix_conv_errno(errno);
	free(rawmode);
	return EV_OK;
}

ev_code_t ev_file_remove(const char *path) {
	if (remove(path) < 0) return evi_unix_conv_errno(errno);
	return EV_OK;
}
ev_code_t ev_file_symlink(const char *path, const char *target) {
	if (symlink(path, target) < 0) return evi_unix_conv_errno(errno);
	return EV_OK;
}
ev_code_t ev_file_hardlink(const char *path, const char *target) {
	if (link(path, target) < 0) return evi_unix_conv_errno(errno);
	return EV_OK;
}
ev_code_t ev_file_readlink(const char *path, char **pres) {
	struct stat stat;
	if (lstat(path, &stat) < 0) return evi_unix_conv_errno(errno);

	char *res = malloc(stat.st_size + 1);
	if (!res) return EV_ENOMEM;

	int n = readlink(path, res, stat.st_size + 1);

	if (n < 0) {
		free(res);
		return evi_unix_conv_errno(errno);
	}

	res[n] = 0;

	*pres = res;
	return EV_OK;
}

ev_code_t ev_file_open(ev_filelist_t fl, ev_fd_t *pres, const char *path, ev_open_flags_t flags, int mode) {
	int fd = -1;

	ev_fd_t res = malloc(sizeof *res);
	if (!res) return EV_ENOMEM;

	fd = open(path, evi_unix_conv_open_flags(flags), mode);
	if (fd < 0) return evi_unix_conv_errno(errno);

	#ifndef EV_USE_LINUX
		if (flags == EV_OPEN_STAT) {
			close(fd);

			char *at = malloc(strlen(path) + 1);
			strcpy(res->impl.at, path);
			evi_unix_mkat(fl, res, at);

			*pres = res;
			return EV_OK;
		}
		else {
			res->impl.is_at = false;
		}
	#endif

	evi_unix_mkfd(fl, res, fd);

	*pres = res;
	return EV_OK;
}
ev_code_t ev_file_read(ev_fd_t fd, char *buff, size_t *n, size_t offset) {
	if (!evi_unix_isfd(fd)) return EV_EBADF;

	ssize_t res = pread(fd->impl.fd, buff, *n, offset);
	if (res < 0) return evi_unix_conv_errno(errno);
	*n = res;
	return EV_OK;
}
ev_code_t ev_file_write(ev_fd_t fd, char *buff, size_t *n, size_t offset) {
	if (!evi_unix_isfd(fd)) return EV_EBADF;

	ssize_t res = pwrite(fd->impl.fd, buff, *n, offset);
	if (res < 0) return evi_unix_conv_errno(errno);
	*n = res;
	return EV_OK;
}
ev_code_t ev_file_chmod(ev_fd_t fd, int mode) {
	if (evi_unix_isfd(fd)) {
		if (fchmod(fd->impl.fd, mode) < 0) return evi_unix_conv_errno(errno);
	}
	#ifndef EV_USE_LINUX
	else {
		if (chmod(hnd->impl.at, mode) < 0) return evi_unix_conv_errno(errno);
	}
	#endif

	return EV_OK;
}
ev_code_t ev_file_chown(ev_fd_t fd, int uid, int gid) {
	if (evi_unix_isfd(fd)) {
		if (fchown(fd->impl.fd, uid, gid) < 0) return evi_unix_conv_errno(errno);
	}
	#ifndef EV_USE_LINUX
	else {
		if (chown(hnd->impl.at, uid, gid) < 0) return evi_unix_conv_errno(errno);
	}
	#endif

	return EV_OK;
}

ev_code_t ev_dir_new(const char *path, int mode) {
	if (mkdir(path, mode) < 0) return evi_unix_conv_errno(errno);
	else return EV_OK;
}
ev_code_t ev_dir_open(ev_filelist_t fl, ev_dir_t *pres, const char *path) {
	ev_dir_t res = malloc(sizeof *res);
	if (!res) return EV_ENOMEM;

	DIR *dir = opendir(path);
	if (!dir) {
		free(res);
		return evi_unix_conv_errno(errno);
	}

	res->impl.dir = dir;

	evi_dlist_add(fl, fl->dir_head, res);
	*pres = res;
	return EV_OK;
}
ev_code_t ev_dir_next(ev_dir_t dir, char **pname) {
	struct dirent *ent;

	do {
		errno = 0;
		ent = readdir(dir->impl.dir);
		if (errno) return evi_unix_conv_errno(errno);

		if (!ent) {
			*pname = NULL;
			return EV_OK;
		}
	}
	while (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."));

	*pname = malloc(strlen(ent->d_name) + 1);
	if (!*pname) return EV_ENOMEM;

	strcpy(*pname, ent->d_name);
	return EV_OK;
}
void ev_dir_close(ev_dir_t dir) {
	while (closedir(dir->impl.dir) < 0) {
		if (errno != EINTR) break;
	}

	evi_dlist_del(fl, dir);
	free(dir);
}

ev_code_t ev_socket_connect(ev_filelist_t fl, ev_fd_t *pres, ev_proto_t proto, ev_addr_t addr, uint16_t port) {
	ev_fd_t client = malloc(sizeof *client);
	if (!client) return EV_ENOMEM;

	struct sockaddr_storage arg_addr;
	int len = evi_unix_conv_addr(addr, port, &arg_addr);

	int sock = evi_unix_socket_new(proto, addr.type);
	if (sock < 0) goto err_socket;

	if (connect(sock, (void*)&arg_addr, len) < 0) goto err_connect;

	evi_unix_mkfd(fl, client, sock);
	*pres = client;
	return EV_OK;

err_socket:
	close(sock);
err_connect:
	free(client);
	return evi_unix_conv_errno(errno);
}
ev_code_t ev_socket_bind(ev_filelist_t fl, ev_fd_t *pres, ev_proto_t proto, ev_addr_t addr, uint16_t port, size_t max_n) {
	ev_fd_t server = malloc(sizeof *server);
	if (!server) return EV_ENOMEM;

	int sock = evi_unix_socket_new(proto, addr.type);
	if (sock < 0) goto err_socket;

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int) { 1 }, sizeof(int)) < 0) goto err_setsockopt;

	struct sockaddr_storage arg_addr;
	int len = evi_unix_conv_addr(addr, port, &arg_addr);

	if (bind(server->impl.fd, (void*)&arg_addr, len) < 0) goto err_bind;
	if (listen(server->impl.fd, max_n) < 0) goto err_listen;

	evi_unix_mkfd(fl, server, sock);
	*pres = server;
	return EV_OK;

err_listen:
err_bind:
err_setsockopt:
	close(sock);
err_socket:
	free(server);
	return evi_unix_conv_errno(errno);
}
ev_code_t ev_socket_accept(ev_filelist_t fl, ev_fd_t server, ev_fd_t *pres, ev_addr_t *paddr, uint16_t *pport) {
	if (!evi_unix_isfd(server)) return EV_EBADF;

	ev_fd_t client = malloc(sizeof *client);
	if (!client) return EV_ENOMEM;

	struct sockaddr_storage addr = {};
	socklen_t addr_len = sizeof addr;

	int res = accept((int)(size_t)server, (void*)&addr, &addr_len);
	if (res < 0) goto err_accept;

	evi_unix_conv_sockaddr(&addr, paddr, pport);

	evi_unix_mkfd(fl, client, res);
	*pres = client;
	return EV_OK;

err_accept:
	free(client);
	return evi_unix_conv_errno(errno);
}

ev_code_t ev_dns_getaddrinfo(ev_addrinfo_t *pres, const char *name, ev_addrinfo_flags_t flags) {
	struct addrinfo hints = { 0 };

	if (flags & EV_AI_IPV4_MAPPED) hints.ai_flags |= EV_AI_IPV4_MAPPED;

	if (flags & EV_AI_IPV6) hints.ai_family = AF_INET6;
	else if (flags & EV_AI_IPV4) hints.ai_family = AF_INET;
	else hints.ai_family = AF_UNSPEC;

	if (flags & EV_AI_BIND) hints.ai_flags |= AI_PASSIVE;
	if (flags & EV_AI_NODNS) hints.ai_flags |= AI_NUMERICHOST;

	struct addrinfo *list = NULL;

	int code;

	// We still want to resolve a valid loopback IP, even if getaddrinfo
	code = getaddrinfo(name, "0", &hints, &list);

	switch (code) {
		case 0: break;
		#ifdef EV_USE_LINUX
			case EAI_NODATA: break;
		#endif
		case EAI_NONAME: break;
		default: return evi_unix_conv_aierr(code);
	}

	size_t n = 0;
	for (struct addrinfo *it = list; it; it = it->ai_next) n++;

	ev_addrinfo_t res = malloc(sizeof *res + sizeof *res->addr * n);
	if (!res) return ENOMEM;

	size_t i = 0;
	for (struct addrinfo *it = list; it; it = it->ai_next) {
		uint16_t port;
		ev_addr_t addr;
		evi_unix_conv_sockaddr((void*)it->ai_addr, &addr, &port);

		bool found = false;

		for (size_t j = 0; j < i; j++) {
			if (ev_addrcmp(addr, res->addr[j])) {
				found = true;
				break;
			}
		}

		if (!found) {
			res->addr[i] = addr;
			i++;
		}
	}

	res->n = i;

	if (list) freeaddrinfo(list);

	*pres = res;
	return EV_OK;
}

// Equivalent to posix's fork then exec
ev_code_t ev_proc_spawn(
	ev_filelist_t fl, ev_proc_t *pres,
	const char **argv, const char **env, const char *cwd,
	ev_fd_t *pin, ev_fd_t *pout, ev_fd_t *perr
) {
	int in_parent = -1, in_child = -1;
	int out_parent = -1, out_child = -1;
	int err_parent = -1, err_child = -1;
	ev_fd_t res_in = NULL, res_out = NULL, res_err = NULL;

	ev_proc_t res = malloc(sizeof *res);

	int status_pipe[2];

	if (pipe(status_pipe) < 0) goto err_pipe_status;
	if (fcntl(status_pipe[0], F_SETFD, FD_CLOEXEC) < 0) goto err_fnctl_status;
	if (fcntl(status_pipe[1], F_SETFD, FD_CLOEXEC) < 0) goto err_fnctl_status;
	if (fcntl(status_pipe[1], F_SETFL, O_NONBLOCK) < 0) goto err_fnctl_status;

	if (pin) {
		if (evi_unix_mkstd(true, &in_parent, &in_child, &res_in) < 0) goto err_mkstd_in;
	}
	if (pout) {
		if (evi_unix_mkstd(false, &out_parent, &out_child, &res_in) < 0) goto err_mkstd_out;
	}
	if (perr) {
		if (evi_unix_mkstd(false, &err_parent, &err_child, &res_in) < 0) goto err_mkstd_err;
	}

	pid_t pid = fork();
	if (pid < 0) goto err_fork;
	if (!pid) { // child
		close(status_pipe[0]);

		if (in_child != -1) {
			if (dup2(in_child, STDIN_FILENO) < 0) goto err_child;
		}
		if (out_child != -1) {
			if (dup2(out_child, STDOUT_FILENO) < 0) goto err_child;
		}
		if (err_child != -1) {
			if (dup2(err_child, STDERR_FILENO) < 0) goto err_child;
		}

		if (in_parent != -1) close(in_parent);
		if (out_parent != -1) close(out_parent);
		if (err_parent != -1) close(err_parent);
		in_parent = out_parent = err_parent = -1;

		if (in_child != -1) close(in_child);
		if (out_child != -1) close(out_child);
		if (err_child != -1) close(err_child);
		in_child = out_child = err_child = -1;

		if (cwd) {
			if (chdir(cwd) < 0) goto err_child;
		}

		sigset_t set;
		sigemptyset(&set);
		if (ev_setmask(SIG_SETMASK, &set, NULL) < 0) goto err_child;

		errno = 0;
		execve(argv[0], (void*)argv, (void*)env);

	err_child: ;
		int err = errno;
		write(status_pipe[1], &err, sizeof err);

		if (in_child != -1) close(in_child);
		if (out_child != -1) close(out_child);
		if (err_child != -1) close(err_child);
		close(status_pipe[0]);

		_exit(127);
	}

	if (in_child != -1) close(in_child);
	if (out_child != -1) close(out_child);
	if (err_child != -1) close(err_child);
	in_child = out_child = err_child = -1;

	close(status_pipe[1]);
	status_pipe[1] = -1;

	int child_code;
	int read_n = read(status_pipe[0], &child_code, sizeof child_code);
	close(status_pipe[0]);
	status_pipe[0] = -1;

	if (read_n < 0) goto err_read_status;
	else if (read_n == 4) {
		assert(read_n == 4);
		errno = child_code;
		goto err_read_status;
	}
	else if (read_n != 0) {
		errno = EPIPE;
		goto err_read_status;
	}

	if (in_parent != -1) {
		evi_unix_mkfd(fl, res_in, in_parent);
		*pin = res_in;
	}
	if (out_parent != -1) {
		evi_unix_mkfd(fl, res_out, out_parent);
		*pout = res_out;
	}
	if (err_parent != -1) {
		evi_unix_mkfd(fl, res_err, err_parent);
		*perr = res_err;
	}

	res->impl.pid = pid;
	evi_dlist_add(fl, fl->proc_head, res);
	*pres = res;
	return 0;

err_read_status:
	// Reap the child upon an error
	if (pid) waitpid(pid, NULL, 0);
err_fork:
	if (err_parent != -1) close(err_parent);
	if (err_child != -1) close(err_child);
err_mkstd_err:
	if (out_parent != -1) close(out_parent);
	if (out_child != -1) close(out_child);
err_mkstd_out:
	if (in_parent != -1) close(in_parent);
	if (in_child != -1) close(in_child);
err_mkstd_in:
err_fnctl_status:
	if (status_pipe[0] != -1) close(status_pipe[0]);
	if (status_pipe[1] != -1) close(status_pipe[1]);
err_pipe_status:
	return evi_unix_conv_errno(errno);
}
ev_code_t ev_proc_wait(ev_proc_t proc, int *psig, int *pcode) {
	int status;
	if (waitpid(proc->impl.pid, &status, 0) < 0) return evi_unix_conv_errno(errno);

	*pcode = -1;
	*psig = -1;

	if (WIFEXITED(status)) {
		*pcode = WEXITSTATUS(status);
	}
	if (WIFSIGNALED(status)) {
		*pcode = WTERMSIG(status);
	}

	free(proc);
	return 0;
}
ev_code_t ev_proc_disown(ev_proc_t proc) {
	// TODO: implement reaper
	free(proc);
	return 0;
}

static void evi_sig_init() {
	// TODO: do with CAS
	if (!_core_sig_init) {
		_core_sig_init = true;

		ev_mutex_new(_core_sig_mut);
		memset(_core_sig_counts, 0, sizeof _core_sig_counts);
		sigemptyset(&_core_sig_set);
	}
}

ev_code_t ev_sig_on(ev_signo_t sig) {
	evi_sig_init();

	ev_mutex_lock(_core_sig_mut);

	if (!_core_sig_counts[sig]) {
		sigset_t old_set = _core_sig_set;

		switch (sig) {
			case EV_SIGINT: sigaddset(&_core_sig_set, SIGINT); break;
			case EV_SIGQUIT: sigaddset(&_core_sig_set, SIGQUIT); break;
			case EV_SIGABRT: sigaddset(&_core_sig_set, SIGABRT); break;
			case EV_SIGTERM: sigaddset(&_core_sig_set, SIGTERM); break;

			case EV_SIGBADMEM:
				sigaddset(&_core_sig_set, SIGSEGV);
				sigaddset(&_core_sig_set, SIGBUS);
				sigaddset(&_core_sig_set, SIGSTKFLT);
				break;
			case EV_SIGBADOP:
				sigaddset(&_core_sig_set, SIGILL);
				sigaddset(&_core_sig_set, SIGFPE);
				sigaddset(&_core_sig_set, SIGSYS);
				break;
			case EV_SIGBADPIPE: sigaddset(&_core_sig_set, SIGPIPE); break;

			case EV_SIGTSIZE: sigaddset(&_core_sig_set, SIGWINCH); break;
			case EV_SIGTLOST: sigaddset(&_core_sig_set, SIGHUP); break;

			case EV_SIGUSR1: sigaddset(&_core_sig_set, SIGUSR1); break;
			case EV_SIGUSR2: sigaddset(&_core_sig_set, SIGUSR2); break;
		}

		if (ev_setmask(SIG_SETMASK, &_core_sig_set, NULL) < 0) {
			_core_sig_set = old_set;
			ev_mutex_unlock(_core_sig_mut);
			return evi_unix_conv_errno(errno);
		}

		// Very bad solution, come up with a better one if u can
		#ifdef EV_USE_URING
			if (signalfd(ev->async->signal_fd, &_sig_set, 0) < 0) {
				ev_setmask(SIG_SETMASK, &old_set, NULL);

				_sig_set = old_set;
				ev_mutex_unlock(_sig_mut);
				return evi_unix_conv_errno(errno);
			}
		#endif
	}

	_core_sig_counts[sig]++;

	ev_mutex_unlock(_core_sig_mut);
	return EV_OK;
}
ev_code_t ev_sig_off(ev_signo_t sig) {
	evi_sig_init();

	ev_mutex_lock(_core_sig_mut);

	if (_core_sig_counts[sig] == 1) {
		sigset_t old_set = _core_sig_set;

		switch (sig) {
			case EV_SIGINT: sigdelset(&_core_sig_set, SIGINT); break;
			case EV_SIGQUIT: sigdelset(&_core_sig_set, SIGQUIT); break;
			case EV_SIGABRT: sigdelset(&_core_sig_set, SIGABRT); break;
			case EV_SIGTERM: sigdelset(&_core_sig_set, SIGTERM); break;

			case EV_SIGBADMEM:
				sigdelset(&_core_sig_set, SIGSEGV);
				sigdelset(&_core_sig_set, SIGBUS);
				sigdelset(&_core_sig_set, SIGSTKFLT);
				break;
			case EV_SIGBADOP:
				sigdelset(&_core_sig_set, SIGILL);
				sigdelset(&_core_sig_set, SIGFPE);
				sigdelset(&_core_sig_set, SIGSYS);
				break;
			case EV_SIGBADPIPE: sigdelset(&_core_sig_set, SIGPIPE); break;

			case EV_SIGTSIZE: sigdelset(&_core_sig_set, SIGWINCH); break;
			case EV_SIGTLOST: sigdelset(&_core_sig_set, SIGHUP); break;

			case EV_SIGUSR1: sigdelset(&_core_sig_set, SIGUSR1); break;
			case EV_SIGUSR2: sigdelset(&_core_sig_set, SIGUSR2); break;
		}

		if (ev_setmask(SIG_SETMASK, &_core_sig_set, NULL) < 0) {
			_core_sig_set = old_set;
			ev_mutex_unlock(_core_sig_mut);
			return evi_unix_conv_errno(errno);
		}

		// Very bad solution, come up with a better one if u can
		#ifdef EV_USE_URING
			if (signalfd(ev->async->signal_fd, &_sig_set, 0) < 0) {
				ev_setmask(SIG_SETMASK, &old_set, NULL);

				_sig_set = old_set;
				ev_mutex_unlock(_sig_mut);
				return evi_unix_conv_errno(errno);
			}
		#endif
	}

	if (_core_sig_counts[sig]) {
		_core_sig_counts[sig]--;
	}

	ev_mutex_unlock(_core_sig_mut);
	return EV_OK;
}
ev_code_t ev_sig_wait(ev_signo_t *pres) {
	evi_sig_init();

	sigset_t old, add_pwr, full;
	sigfillset(&full);
	sigemptyset(&add_pwr);
	sigaddset(&add_pwr, SIGPWR);
	if (ev_setmask(SIG_BLOCK, &add_pwr, &old) < 0) return evi_unix_conv_errno(errno);

	int res;
	while (true) {
		if (sigwait(&full, &res) < 0) {
			ev_setmask(SIG_SETMASK, &old, NULL);
			return evi_unix_conv_errno(errno);
		}

		if (res == SIGPWR) {
			ev_setmask(SIG_SETMASK, &old, NULL);
			return EV_EINTR;
		}

		int sig = evi_unix_conv_signal(res);
		if (sig < 0) continue;

		*pres = sig;
		ev_setmask(SIG_SETMASK, &old, NULL);
		return EV_OK;
	}
}

ev_code_t ev_getpath(ev_path_type_t type, char **pres) {
	switch (type) {
		case EV_PATH_HOME: {
			char *res = evi_generic_getenvpath(NULL);
			if (!res) return evi_unix_conv_errno(errno);

			*pres = res;
			return EV_OK;
		}
		case EV_PATH_CACHE: {
			char *res = evi_unix_getpath("XDG_CACHE_HOME", "/.cache");
			if (!res) return evi_unix_conv_errno(errno);

			*pres = res;
			return EV_OK;
		}
		case EV_PATH_CONFIG: {
			char *res = evi_unix_getpath("XDG_CONFIG_HOME", "/.config");
			if (!res) return evi_unix_conv_errno(errno);

			*pres = res;
			return EV_OK;
		}
		case EV_PATH_DATA: {
			char *res = evi_unix_getpath("XDG_DATA_HOME", "/.local/share");
			if (!res) return evi_unix_conv_errno(errno);

			*pres = res;
			return EV_OK;
		}
		case EV_PATH_RUNTIME: {
			const char *res;

			const char *env = getenv("XDG_RUNTIME_DIR");
			if (env && *env) res = env;
			else res = "/tmp";

			*pres = malloc(strlen(res) + 1);
			if (!*pres) return evi_unix_conv_errno(errno);

			strcpy(*pres, res);
			return EV_OK;
		}
		case EV_PATH_CWD: {
			char *buff = malloc(PATH_MAX);
			size_t buffn = PATH_MAX;
			if (!buff) return EV_ENOMEM;

			while (true) {
				errno = 0;
				if (getcwd(buff, buffn)) break;
				if (errno != ERANGE) {
					free(buff);
					return evi_unix_conv_errno(errno);
				}

				buffn *= 2;
				free(buff);
				buff = malloc(buffn);
				if (!buff) return EV_ENOMEM;
			}

			*pres = realloc(buff, strlen(buff) + 1);
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
		if (unsetenv(name) < 0) return evi_unix_conv_errno(errno);
	}
	else {
		if (setenv(name, val, true) < 0) return evi_unix_conv_errno(errno);
	}

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

ev_time_t ev_time(ev_clock_t clock) {
	struct timespec res;
	int err;
	switch (clock) {
		case EV_CLOCK_REALTIME: err = clock_gettime(CLOCK_REALTIME, &res); break;
		case EV_CLOCK_MONOTIME: err = clock_gettime(CLOCK_MONOTONIC, &res); break;
		case EV_CLOCK_CPUTIME: {
			#ifdef EV_USE_LINUX
				err = clock_gettime(CLOCK_MONOTONIC, &res);
			#else
				struct tms buff;
				times(&buff);
				res.tv_sec = (buff.tms_stime + buff.tms_utime) / CLOCKS_PER_SEC;
				res.tv_nsec = ((buff.tms_stime + buff.tms_utime) % CLOCKS_PER_SEC) * (1000000000 / CLOCKS_PER_SEC);
			#endif
			err = 0;
			break;
		}
	}

	assert(err >= 0 && "retrieving clock failed");

	return (ev_time_t) { .sec = res.tv_sec, .nsec = res.tv_nsec };
}
void ev_timesleep(ev_time_t until) {
	struct timespec req = { .tv_sec = until.sec, .tv_nsec = until.nsec };
	while (true) {
		if (nanosleep(&req, NULL) == 0) break;
		if (errno == EINTR) continue;
	}
}
