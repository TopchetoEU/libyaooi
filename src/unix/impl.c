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

#include <yaooi/io.h>
#include <yaooi/conf.h>
#include <yaooi/errno.h>
#include <yaooi/signo.h>

#include "../utils/multithread.h"

#include "./impl.h" // IWYU pragma: export

#include "../time.c"
#include "./async.c" // IWYU pragma: export

#ifndef __USE_GNU
	extern char **environ;
#endif

static bool _core_sig_init = false;
static yo_mutex_t _core_sig_mut;
static size_t _core_sig_counts[YO_SIGUSR2 + 1];
static sigset_t _core_sig_set;

static char *_yoi_generic_getenvpath(const char *suffix) {
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
static char *_yoi_unix_getpath(const char *envname, const char *suffix) {
	const char *env = getenv(envname);
	if (env && *env) {
		char *res = malloc(strlen(env) + 1);
		if (!res) return NULL;

		strcpy(res, env);
		return res;
	}

	return _yoi_generic_getenvpath(suffix);
}

// Equivalent to socket(). The created socket is stored in `socket`
static int _yoi_unix_socket_new(yo_proto_t proto, yo_addr_type_t addr) {
	return socket(
		addr == YO_ADDR_IPV4 ? AF_INET : AF_INET6,
		proto == YO_PROTO_UDP ? SOCK_DGRAM : SOCK_STREAM,
		proto == YO_PROTO_UDP ? IPPROTO_UDP : IPPROTO_TCP
	);
}

static int _yoi_unix_mkstd(bool in, int *pparent, int *pchild, yo_fd_t *pres) {
	yo_fd_t res = malloc(sizeof *res);
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

static void _yoi_sig_init() {
	// TODO: do with CAS
	if (!_core_sig_init) {
		_core_sig_init = true;

		yo_mutex_new(_core_sig_mut);
		memset(_core_sig_counts, 0, sizeof _core_sig_counts);
		sigemptyset(&_core_sig_set);
	}
}

static bool yoi_unix_isfd(yo_fd_t res) {
	res->head = NULL;

	#ifndef YO_USE_LINUX
		return !res->is_at;
	#else
		(void)res;
		return true;
	#endif
}
static void yoi_unix_mkfd(yo_fd_t res, int fd) {
	res->owned = true;
	res->fd = fd;
	res->head = NULL;
	#ifndef YO_USE_LINUX
		res->is_at = false;
	#endif
}
#ifndef YO_USE_LINUX
static bool yoi_unix_mkat(yo_fd_t res, const char *path) {
	char *at = malloc(strlen(path) + 1);
	if (!at) return false;

	strcpy(res->at, path);

	res->owned = true;
	res->at = at;
	res->is_at = true;

	return true;
}
#endif

static int yoi_unix_conv_open_flags(yo_open_flags_t flags) {
	int res = 0;

	if (flags & YO_OPEN_STAT) {
		#ifdef YO_USE_LINUX
			res |= O_PATH;
		#endif
	}
	else {
		if (flags & YO_OPEN_APPEND) {
			flags |= YO_OPEN_WRITE;
			res |= O_APPEND;
		}

		if (flags & YO_OPEN_WRITE) {
			if (flags & YO_OPEN_READ) {
				res |= O_RDWR;
			}
			else {
				res |= O_WRONLY;
			}
		}
		else if (flags & YO_OPEN_READ) {
			res |= O_RDONLY;
		}
	}

	if (flags & YO_OPEN_CREATE) res |= O_CREAT;
	if (flags & YO_OPEN_TRUNC) res |= O_TRUNC;
	if (flags & YO_OPEN_DIRECT) res |= O_SYNC;
	if (flags & YO_OPEN_NOFOLLOW) res |= O_NOFOLLOW;
	if (!(flags & YO_OPEN_SHARED)) res |= O_CLOEXEC;

	return res;
}
static void yoi_unix_conv_stat_mode(int mode, yo_stat_t *dst) {
	switch (mode & S_IFMT) {
		case S_IFREG: dst->type = YO_STAT_REG; break;
		case S_IFDIR: dst->type = YO_STAT_DIR; break;
		case S_IFLNK: dst->type = YO_STAT_LINK; break;
		case S_IFSOCK: dst->type = YO_STAT_SOCK; break;
		case S_IFIFO: dst->type = YO_STAT_FIFO; break;
		case S_IFCHR: dst->type = YO_STAT_CHAR; break;
		case S_IFBLK: dst->type = YO_STAT_BLK; break;
		default: dst->type = -1; break;
	}

	dst->mode = mode & ~S_IFMT;
}
static void yoi_unix_conv_stat(yo_stat_t *dst, struct stat *src) {
	yoi_unix_conv_stat_mode(src->st_mode, dst);

	dst->mode = src->st_mode & ~S_IFMT;
	dst->uid = src->st_uid;
	dst->gid = src->st_gid;
	dst->atime = (yo_time_t) { .sec = src->st_atim.tv_sec, .nsec = src->st_atim.tv_nsec };
	dst->ctime = (yo_time_t) { .sec = src->st_ctim.tv_sec, .nsec = src->st_ctim.tv_nsec };
	dst->mtime = (yo_time_t) { .sec = src->st_mtim.tv_sec, .nsec = src->st_mtim.tv_nsec };
	dst->size = src->st_size;
	dst->inode = src->st_ino;
	dst->links = src->st_nlink;
	dst->blksize = src->st_blksize;
}

static int yoi_unix_conv_signal(int sig) {
	switch (sig) {
		case SIGHUP: return YO_SIGTLOST;
		case SIGINT: return YO_SIGINT;
		case SIGQUIT: return YO_SIGQUIT;
		case SIGILL: return YO_SIGBADOP;
		case SIGABRT: return YO_SIGABRT;
		case SIGBUS: return YO_SIGBADMEM;
		case SIGFPE: return YO_SIGBADOP;
		case SIGUSR1: return YO_SIGUSR1;
		case SIGSEGV: return YO_SIGBADMEM;
		case SIGUSR2: return YO_SIGUSR2;
		case SIGPIPE: return YO_SIGBADPIPE;
		case SIGTERM: return YO_SIGTERM;
		case SIGSTKFLT: return YO_SIGBADMEM;
		case SIGWINCH: return YO_SIGTSIZE;
		case SIGSYS: return YO_SIGBADOP;
	}
	return -1;
}
static yo_code_t yoi_unix_conv_errno(int unixerr) {
	switch (unixerr) {
		case EPERM: return YO_EPERM;
		case ENOENT: return YO_ENOENT;
		case ESRCH: return YO_ESRCH;
		case EINTR: return YO_EINTR;
		case EIO: return YO_EIO;
		case ENXIO: return YO_ENXIO;
		case E2BIG: return YO_E2BIG;
		case ENOEXEC: return YO_ENOEXEC;
		case EBADF: return YO_EBADF;
		case ECHILD: return YO_ECHILD;
		case EAGAIN: return YO_EAGAIN;
		case ENOMEM: return YO_ENOMEM;
		case EACCES: return YO_EACCES;
		case EFAULT: return YO_EFAULT;
		case EBUSY: return YO_EBUSY;
		case EEXIST: return YO_EEXIST;
		case EXDEV: return YO_EXDEV;
		case ENODEV: return YO_ENODEV;
		case ENOTDIR: return YO_ENOTDIR;
		case EISDIR: return YO_EISDIR;
		case EINVAL: return YO_EINVAL;
		case ENFILE: return YO_ENFILE;
		case EMFILE: return YO_EMFILE;
		case ENOTTY: return YO_ENOTTY;
		case ETXTBSY: return YO_ETXTBSY;
		case EFBIG: return YO_EFBIG;
		case ENOSPC: return YO_ENOSPC;
		case ESPIPE: return YO_ESPIPE;
		case EROFS: return YO_EROFS;
		case EMLINK: return YO_EMLINK;
		case EPIPE: return YO_EPIPE;
		case ERANGE: return YO_ERANGE;
		case EDEADLK: return YO_EDEADLK;
		case ENAMETOOLONG: return YO_ENAMETOOLONG;
		case ENOLCK: return YO_ENOLCK;
		case ENOSYS: return YO_ENOSYS;
		case ENOTEMPTY: return YO_ENOTEMPTY;
		case ELOOP: return YO_ELOOP;
		case EUNATCH: return YO_EUNATCH;
		case ENODATA: return YO_ENODATA;
		case ENONET: return YO_ENONET;
		case ECOMM: return YO_ECOMM;
		case EPROTO: return YO_EPROTO;
		case EOVERFLOW: return YO_EOVERFLOW;
		case ENOTUNIQ: return YO_ENOTUNIQ;
		case ELIBBAD: return YO_ELIBBAD;
		case EILSEQ: return YO_EILSEQ;
		case ENOTSOCK: return YO_ENOTSOCK;
		case EDESTADDRREQ: return YO_EDESTADDRREQ;
		case EMSGSIZE: return YO_EMSGSIZE;
		case EPROTOTYPE: return YO_EPROTOTYPE;
		case ENOPROTOOPT: return YO_ENOPROTOOPT;
		case EPROTONOSUPPORT: return YO_EPROTONOSUPPORT;
		case ESOCKTNOSUPPORT: return YO_ESOCKTNOSUPPORT;
		case ENOTSUP: return YO_ENOTSUP;
		case EPFNOSUPPORT: return YO_EPFNOSUPPORT;
		case EAFNOSUPPORT: return YO_EAFNOSUPPORT;
		case EADDRINUSE: return YO_EADDRINUSE;
		case EADDRNOTAVAIL: return YO_EADDRNOTAVAIL;
		case ENETDOWN: return YO_ENETDOWN;
		case ENETUNREACH: return YO_ENETUNREACH;
		case ECONNABORTED: return YO_ECONNABORTED;
		case ECONNRESET: return YO_ECONNRESET;
		case ENOBUFS: return YO_ENOBUFS;
		case EISCONN: return YO_EISCONN;
		case ENOTCONN: return YO_ENOTCONN;
		case ESHUTDOWN: return YO_ESHUTDOWN;
		case ETIMEDOUT: return YO_ETIMEDOUT;
		case ECONNREFUSED: return YO_ECONNREFUSED;
		case EHOSTDOWN: return YO_EHOSTDOWN;
		case EHOSTUNREACH: return YO_EHOSTUNREACH;
		case EALREADY: return YO_EALREADY;
		case EREMOTEIO: return YO_EREMOTEIO;
		case ENOMEDIUM: return YO_ENOMEDIUM;
		case ECANCELED: return YO_ECANCELED;
		case 0: return YO_OK;
		case -1: return YO_EUNKNOWN;
		default: return YO_EUNKNOWN;
	}
}
static yo_code_t yoi_unix_conv_aierr(int aierr) {
	switch (aierr) {
		case EAI_BADFLAGS: return YO_EAI_BADFLAGS;
		case EAI_NONAME: return YO_EAI_NONAME;
		case EAI_AGAIN: return YO_EAI_AGAIN;
		case EAI_FAIL: return YO_EAI_FAIL;
		case EAI_FAMILY: return YO_EAI_FAMILY;
		case EAI_SOCKTYPE: return YO_EAI_SOCKTYPE;
		case EAI_SERVICE: return YO_EAI_SERVICE;
		case EAI_MEMORY: return YO_EAI_MEMORY;
		case EAI_OVERFLOW: return YO_EAI_OVERFLOW;
		#ifdef YO_USE_LINUX
			case EAI_NODATA: return YO_EAI_NODATA;
			case EAI_ADDRFAMILY: return YO_EAI_ADDRFAMILY;
			#ifdef EAI_CANCELED // pesky gnu extensions
				case EAI_CANCELED: return YO_EAI_CANCELED;
			#endif
		#endif

		default: return YO_EUNKNOWN;
	}
}

static int yoi_unix_conv_addr(yo_addr_t addr, uint16_t port, struct sockaddr_storage *pres) {
	if (addr.type == YO_ADDR_IPV4) {
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
static void yoi_unix_conv_sockaddr(struct sockaddr_storage *sockaddr, yo_addr_t *pres, uint16_t *pport) {
	if (sockaddr->ss_family == AF_INET) {
		struct sockaddr_in *sockaddr_in = (void*)sockaddr;

		*pport = ntohs(sockaddr_in->sin_port);
		pres->type = YO_ADDR_IPV4;
		memcpy(pres->v4, &sockaddr_in->sin_addr, sizeof sockaddr_in->sin_addr);
	}
	else {
		struct sockaddr_in6 *sockaddr_in6 = (void*)sockaddr;

		*pport = ntohs(sockaddr_in6->sin6_port);
		pres->type = YO_ADDR_IPV6;
		memcpy(pres->v6, &sockaddr_in6->sin6_addr, sizeof sockaddr_in6->sin6_addr);

		for (size_t i = 0; i < 8; i++) {
			pres->v6[i] = ntohs(pres->v6[i]);
		}
	}
}

yo_code_t yo_fd_new(yo_fd_t *pres, uint64_t fd, bool owned) {
	yo_fd_t res = malloc(sizeof *res);
	if (!res) return YO_ENOMEM;

	yoi_unix_mkfd(res, fd);
	res->owned = owned;
	*pres = res;

	return YO_OK;
}
void yo_fd_close(yo_fd_t fd) {
	#ifdef yoi_unix_onclose
		yoi_unix_onclose(fd);
	#endif

	if (fd->owned) {
		#ifndef YO_USE_LINUX
		if (fd->is_at) {
			free(fd->at);
		}
		else
		#endif
		{
			while (close(fd->fd) < 0) {
				if (errno != EINTR) return;
			}
		}
	}

	free(fd);
}

yo_code_t yo_read(yo_fd_t fd, char *buff, size_t *pn) {
	if (!yoi_unix_isfd(fd)) return YO_EBADF;

	ssize_t n = read(fd->fd, buff, *pn);
	if (n < 0) return yoi_unix_conv_errno(errno);

	*pn = n;
	return YO_OK;
}
yo_code_t yo_write(yo_fd_t fd, char *buff, size_t *pn) {
	if (!yoi_unix_isfd(fd)) return YO_EBADF;

	ssize_t n = write(fd->fd, buff, *pn);
	if (n < 0) return yoi_unix_conv_errno(errno);

	*pn = n;
	return YO_OK;
}
yo_code_t yo_sync(yo_fd_t fd) {
	if (!yoi_unix_isfd(fd)) return YO_EBADF;
	return yoi_unix_conv_errno(fsync(fd->fd));
}
yo_code_t yo_stat(yo_fd_t fd, yo_stat_t *buff) {
	struct stat res;

	if (yoi_unix_isfd(fd)) {
		if (fstat(fd->fd, &res) < 0) return yoi_unix_conv_errno(errno);
	}
	#ifndef YO_USE_LINUX
	else {
		// TODO: respect NOFOLLOW
		if (lstat(fd->at, &res) < 0) return yoi_unix_conv_errno(errno);
	}
	#endif

	yoi_unix_conv_stat(buff, &res);
	return YO_OK;
}

yo_code_t yo_tty_in(yo_fd_t *pres) {
	return yo_fd_new(pres, STDIN_FILENO, false);
}
yo_code_t yo_tty_out(yo_fd_t *pres) {
	return yo_fd_new(pres, STDOUT_FILENO, false);
}
yo_code_t yo_tty_err(yo_fd_t *pres) {
	return yo_fd_new(pres, STDERR_FILENO, false);
}
yo_code_t yo_tty_raw(yo_fd_t tty, yo_tty_raw_t *pres) {
	if (!yoi_unix_isfd(tty)) return YO_EBADF;

	yo_tty_raw_t res = malloc(sizeof *res);
	if (!res) return YO_ENOMEM;

	res->fd = tty->fd;

	if (tcgetattr(res->fd, &res->pryo_mode) < 0) {
		free(res);
		return yoi_unix_conv_errno(errno);
	}
	struct termios raw = res->pryo_mode;

	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= CS8;
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

	raw.c_cc[VMIN]  = 1;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(res->fd, TCSAFLUSH, &raw) < 0) {
		free(res);
		return yoi_unix_conv_errno(errno);
	}

	*pres = res;
	return YO_OK;
}
yo_code_t yo_tty_rawend(yo_tty_raw_t rawmode) {
	if (tcsetattr(rawmode->fd, TCSAFLUSH, &rawmode->pryo_mode) < 0) return yoi_unix_conv_errno(errno);
	free(rawmode);
	return YO_OK;
}

yo_code_t yo_file_remove(const char *path) {
	if (remove(path) < 0) return yoi_unix_conv_errno(errno);
	return YO_OK;
}
yo_code_t yo_file_symlink(const char *path, const char *target) {
	if (symlink(path, target) < 0) return yoi_unix_conv_errno(errno);
	return YO_OK;
}
yo_code_t yo_file_hardlink(const char *path, const char *target) {
	if (link(path, target) < 0) return yoi_unix_conv_errno(errno);
	return YO_OK;
}
yo_code_t yo_file_readlink(const char *path, char **pres) {
	struct stat stat;
	if (lstat(path, &stat) < 0) return yoi_unix_conv_errno(errno);

	char *res = malloc(stat.st_size + 1);
	if (!res) return YO_ENOMEM;

	int n = readlink(path, res, stat.st_size + 1);

	if (n < 0) {
		free(res);
		return yoi_unix_conv_errno(errno);
	}

	res[n] = 0;

	*pres = res;
	return YO_OK;
}

yo_code_t yo_file_open(yo_fd_t *pres, const char *path, yo_open_flags_t flags, int mode) {
	int fd = -1;

	yo_fd_t res = malloc(sizeof *res);
	if (!res) return YO_ENOMEM;

	fd = open(path, yoi_unix_conv_open_flags(flags), mode);
	if (fd < 0) return yoi_unix_conv_errno(errno);

	#ifndef YO_USE_LINUX
		if (flags == YO_OPEN_STAT) {
			close(fd);

			if (!yoi_unix_mkat(res, path)) return YO_ENOMEM;

			*pres = res;
			return YO_OK;
		}
		else {
			res->is_at = false;
		}
	#endif

	yoi_unix_mkfd(res, fd);

	*pres = res;
	return YO_OK;
}
yo_code_t yo_file_read(yo_fd_t fd, char *buff, size_t *n, size_t offset) {
	if (!yoi_unix_isfd(fd)) return YO_EBADF;

	ssize_t res = pread(fd->fd, buff, *n, offset);
	if (res < 0) return yoi_unix_conv_errno(errno);
	*n = res;
	return YO_OK;
}
yo_code_t yo_file_write(yo_fd_t fd, char *buff, size_t *n, size_t offset) {
	if (!yoi_unix_isfd(fd)) return YO_EBADF;

	ssize_t res = pwrite(fd->fd, buff, *n, offset);
	if (res < 0) return yoi_unix_conv_errno(errno);
	*n = res;
	return YO_OK;
}
yo_code_t yo_file_chmod(yo_fd_t fd, int mode) {
	if (yoi_unix_isfd(fd)) {
		if (fchmod(fd->fd, mode) < 0) return yoi_unix_conv_errno(errno);
	}
	#ifndef YO_USE_LINUX
	else {
		if (chmod(fd->at, mode) < 0) return yoi_unix_conv_errno(errno);
	}
	#endif

	return YO_OK;
}
yo_code_t yo_file_chown(yo_fd_t fd, int uid, int gid) {
	if (yoi_unix_isfd(fd)) {
		if (fchown(fd->fd, uid, gid) < 0) return yoi_unix_conv_errno(errno);
	}
	#ifndef YO_USE_LINUX
	else {
		if (chown(fd->at, uid, gid) < 0) return yoi_unix_conv_errno(errno);
	}
	#endif

	return YO_OK;
}

yo_code_t yo_dir_new(const char *path, int mode) {
	if (mkdir(path, mode) < 0) return yoi_unix_conv_errno(errno);
	else return YO_OK;
}
yo_code_t yo_dir_open(yo_dir_t *pres, const char *path) {
	yo_dir_t res = malloc(sizeof *res);
	if (!res) return YO_ENOMEM;

	DIR *dir = opendir(path);
	if (!dir) {
		free(res);
		return yoi_unix_conv_errno(errno);
	}

	res->dir = dir;

	*pres = res;
	return YO_OK;
}
yo_code_t yo_dir_next(yo_dir_t dir, char **pname) {
	struct dirent *ent;

	do {
		errno = 0;
		ent = readdir(dir->dir);
		if (errno) return yoi_unix_conv_errno(errno);

		if (!ent) {
			*pname = NULL;
			return YO_OK;
		}
	}
	while (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."));

	*pname = malloc(strlen(ent->d_name) + 1);
	if (!*pname) return YO_ENOMEM;

	strcpy(*pname, ent->d_name);
	return YO_OK;
}
void yo_dir_close(yo_dir_t dir) {
	while (closedir(dir->dir) < 0) {
		if (errno != EINTR) break;
	}

	free(dir);
}

yo_code_t yo_socket_connect(yo_fd_t *pres, yo_proto_t proto, yo_addr_t addr, uint16_t port) {
	yo_fd_t client = malloc(sizeof *client);
	if (!client) return YO_ENOMEM;

	struct sockaddr_storage arg_addr;
	int len = yoi_unix_conv_addr(addr, port, &arg_addr);

	int sock = _yoi_unix_socket_new(proto, addr.type);
	if (sock < 0) goto err_socket;

	if (connect(sock, (void*)&arg_addr, len) < 0) goto err_connect;

	yoi_unix_mkfd(client, sock);
	*pres = client;
	return YO_OK;

err_socket:
	close(sock);
err_connect:
	free(client);
	return yoi_unix_conv_errno(errno);
}
yo_code_t yo_socket_bind(yo_fd_t *pres, yo_proto_t proto, yo_addr_t addr, uint16_t port, size_t max_n) {
	yo_fd_t server = malloc(sizeof *server);
	if (!server) return YO_ENOMEM;

	int sock = _yoi_unix_socket_new(proto, addr.type);
	if (sock < 0) goto err_socket;

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int) { 1 }, sizeof(int)) < 0) goto err_setsockopt;

	struct sockaddr_storage arg_addr;
	int len = yoi_unix_conv_addr(addr, port, &arg_addr);

	if (bind(server->fd, (void*)&arg_addr, len) < 0) goto err_bind;
	if (listen(server->fd, max_n) < 0) goto err_listen;

	yoi_unix_mkfd(server, sock);
	*pres = server;
	return YO_OK;

err_listen:
err_bind:
err_setsockopt:
	close(sock);
err_socket:
	free(server);
	return yoi_unix_conv_errno(errno);
}
yo_code_t yo_socket_accept(yo_fd_t server, yo_fd_t *pres, yo_addr_t *paddr, uint16_t *pport) {
	if (!yoi_unix_isfd(server)) return YO_EBADF;

	yo_fd_t client = malloc(sizeof *client);
	if (!client) return YO_ENOMEM;

	struct sockaddr_storage addr = {};
	socklen_t addr_len = sizeof addr;

	int res = accept((int)(size_t)server, (void*)&addr, &addr_len);
	if (res < 0) goto err_accept;

	yoi_unix_conv_sockaddr(&addr, paddr, pport);

	yoi_unix_mkfd(client, res);
	*pres = client;
	return YO_OK;

err_accept:
	free(client);
	return yoi_unix_conv_errno(errno);
}

yo_code_t yo_dns_getaddrinfo(yo_addrinfo_t *pres, const char *name, yo_addrinfo_flags_t flags) {
	struct addrinfo hints = { 0 };

	if (flags & YO_AI_IPV4_MAPPED) hints.ai_flags |= YO_AI_IPV4_MAPPED;

	if (flags & YO_AI_IPV6) hints.ai_family = AF_INET6;
	else if (flags & YO_AI_IPV4) hints.ai_family = AF_INET;
	else hints.ai_family = AF_UNSPEC;

	if (flags & YO_AI_BIND) hints.ai_flags |= AI_PASSIVE;
	if (flags & YO_AI_NODNS) hints.ai_flags |= AI_NUMERICHOST;

	struct addrinfo *list = NULL;

	int code;

	// We still want to resolve a valid loopback IP, even if getaddrinfo
	code = getaddrinfo(name, "0", &hints, &list);

	switch (code) {
		case 0: break;
		#ifdef YO_USE_LINUX
			case EAI_NODATA: break;
		#endif
		case EAI_NONAME: break;
		default: return yoi_unix_conv_aierr(code);
	}

	size_t n = 0;
	for (struct addrinfo *it = list; it; it = it->ai_next) n++;

	yo_addrinfo_t res = malloc(sizeof *res + sizeof *res->addr * n);
	if (!res) return ENOMEM;

	size_t i = 0;
	for (struct addrinfo *it = list; it; it = it->ai_next) {
		uint16_t port;
		yo_addr_t addr;
		yoi_unix_conv_sockaddr((void*)it->ai_addr, &addr, &port);

		bool found = false;

		for (size_t j = 0; j < i; j++) {
			if (yo_addrcmp(addr, res->addr[j])) {
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
	return YO_OK;
}

// Equivalent to posix's fork then exec
yo_code_t yo_proc_spawn(
	yo_proc_t *pres, yo_spawn_flags_t flags,
	const char **argv, const char **env, const char *cwd,
	yo_fd_t *pin, yo_fd_t *pout, yo_fd_t *perr
) {
	(void)flags;

	int in_parent = -1, in_child = -1;
	int out_parent = -1, out_child = -1;
	int err_parent = -1, err_child = -1;
	yo_fd_t res_in = NULL, res_out = NULL, res_err = NULL;

	yo_proc_t res = malloc(sizeof *res);

	int status_pipe[2];

	if (pipe(status_pipe) < 0) goto err_pipe_status;
	if (fcntl(status_pipe[0], F_SETFD, FD_CLOEXEC) < 0) goto err_fnctl_status;
	if (fcntl(status_pipe[1], F_SETFD, FD_CLOEXEC) < 0) goto err_fnctl_status;
	if (fcntl(status_pipe[1], F_SETFL, O_NONBLOCK) < 0) goto err_fnctl_status;

	if (pin) {
		if (_yoi_unix_mkstd(true, &in_parent, &in_child, &res_in) < 0) goto err_mkstd_in;
	}
	if (pout) {
		if (_yoi_unix_mkstd(false, &out_parent, &out_child, &res_out) < 0) goto err_mkstd_out;
	}
	if (perr) {
		if (_yoi_unix_mkstd(false, &err_parent, &err_child, &res_err) < 0) goto err_mkstd_err;
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
		if (yo_setmask(SIG_SETMASK, &set, NULL) < 0) goto err_child;

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
		yoi_unix_mkfd(res_in, in_parent);
		*pin = res_in;
	}
	if (out_parent != -1) {
		yoi_unix_mkfd(res_out, out_parent);
		*pout = res_out;
	}
	if (err_parent != -1) {
		yoi_unix_mkfd(res_err, err_parent);
		*perr = res_err;
	}

	res->pid = pid;
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
	return yoi_unix_conv_errno(errno);
}
yo_code_t yo_proc_wait(yo_proc_t proc, int *psig, int *pcode) {
	int status;
	if (waitpid(proc->pid, &status, 0) < 0) return yoi_unix_conv_errno(errno);

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
yo_code_t yo_proc_disown(yo_proc_t proc) {
	// TODO: implement reaper
	free(proc);
	return 0;
}

yo_code_t yo_sig_on(yo_signo_t sig) {
	_yoi_sig_init();

	yo_mutex_lock(_core_sig_mut);

	if (!_core_sig_counts[sig]) {
		sigset_t old_set = _core_sig_set;

		switch (sig) {
			case YO_SIGINT: sigaddset(&_core_sig_set, SIGINT); break;
			case YO_SIGQUIT: sigaddset(&_core_sig_set, SIGQUIT); break;
			case YO_SIGABRT: sigaddset(&_core_sig_set, SIGABRT); break;
			case YO_SIGTERM: sigaddset(&_core_sig_set, SIGTERM); break;

			case YO_SIGBADMEM:
				sigaddset(&_core_sig_set, SIGSEGV);
				sigaddset(&_core_sig_set, SIGBUS);
				sigaddset(&_core_sig_set, SIGSTKFLT);
				break;
			case YO_SIGBADOP:
				sigaddset(&_core_sig_set, SIGILL);
				sigaddset(&_core_sig_set, SIGFPE);
				sigaddset(&_core_sig_set, SIGSYS);
				break;
			case YO_SIGBADPIPE: sigaddset(&_core_sig_set, SIGPIPE); break;

			case YO_SIGTSIZE: sigaddset(&_core_sig_set, SIGWINCH); break;
			case YO_SIGTLOST: sigaddset(&_core_sig_set, SIGHUP); break;

			case YO_SIGUSR1: sigaddset(&_core_sig_set, SIGUSR1); break;
			case YO_SIGUSR2: sigaddset(&_core_sig_set, SIGUSR2); break;
		}

		if (yo_setmask(SIG_SETMASK, &_core_sig_set, NULL) < 0) {
			_core_sig_set = old_set;
			yo_mutex_unlock(_core_sig_mut);
			return yoi_unix_conv_errno(errno);
		}

		// // Very bad solution, come up with a better one if u can
		// #ifdef YO_USE_URING
		// 	if (signalfd(ev->async->signal_fd, &_sig_set, 0) < 0) {
		// 		yo_setmask(SIG_SETMASK, &old_set, NULL);

		// 		_sig_set = old_set;
		// 		yo_mutex_unlock(_sig_mut);
		// 		return yoi_unix_conv_errno(errno);
		// 	}
		// #endif
	}

	_core_sig_counts[sig]++;

	yo_mutex_unlock(_core_sig_mut);
	return YO_OK;
}
yo_code_t yo_sig_off(yo_signo_t sig) {
	_yoi_sig_init();

	yo_mutex_lock(_core_sig_mut);

	if (_core_sig_counts[sig] == 1) {
		sigset_t old_set = _core_sig_set;

		switch (sig) {
			case YO_SIGINT: sigdelset(&_core_sig_set, SIGINT); break;
			case YO_SIGQUIT: sigdelset(&_core_sig_set, SIGQUIT); break;
			case YO_SIGABRT: sigdelset(&_core_sig_set, SIGABRT); break;
			case YO_SIGTERM: sigdelset(&_core_sig_set, SIGTERM); break;

			case YO_SIGBADMEM:
				sigdelset(&_core_sig_set, SIGSEGV);
				sigdelset(&_core_sig_set, SIGBUS);
				sigdelset(&_core_sig_set, SIGSTKFLT);
				break;
			case YO_SIGBADOP:
				sigdelset(&_core_sig_set, SIGILL);
				sigdelset(&_core_sig_set, SIGFPE);
				sigdelset(&_core_sig_set, SIGSYS);
				break;
			case YO_SIGBADPIPE: sigdelset(&_core_sig_set, SIGPIPE); break;

			case YO_SIGTSIZE: sigdelset(&_core_sig_set, SIGWINCH); break;
			case YO_SIGTLOST: sigdelset(&_core_sig_set, SIGHUP); break;

			case YO_SIGUSR1: sigdelset(&_core_sig_set, SIGUSR1); break;
			case YO_SIGUSR2: sigdelset(&_core_sig_set, SIGUSR2); break;
		}

		if (yo_setmask(SIG_SETMASK, &_core_sig_set, NULL) < 0) {
			_core_sig_set = old_set;
			yo_mutex_unlock(_core_sig_mut);
			return yoi_unix_conv_errno(errno);
		}

		// // Very bad solution, come up with a better one if u can
		// #ifdef YO_USE_URING
		// 	if (signalfd(ev->async->signal_fd, &_sig_set, 0) < 0) {
		// 		yo_setmask(SIG_SETMASK, &old_set, NULL);

		// 		_sig_set = old_set;
		// 		yo_mutex_unlock(_sig_mut);
		// 		return yoi_unix_conv_errno(errno);
		// 	}
		// #endif
	}

	if (_core_sig_counts[sig]) {
		_core_sig_counts[sig]--;
	}

	yo_mutex_unlock(_core_sig_mut);
	return YO_OK;
}
yo_code_t yo_sig_wait(yo_signo_t *pres) {
	_yoi_sig_init();

	sigset_t old, add_pwr, full;
	sigfillset(&full);
	sigemptyset(&add_pwr);
	sigaddset(&add_pwr, SIGPWR);
	if (yo_setmask(SIG_BLOCK, &add_pwr, &old) < 0) return yoi_unix_conv_errno(errno);

	int res;
	while (true) {
		if (sigwait(&full, &res) < 0) {
			yo_setmask(SIG_SETMASK, &old, NULL);
			return yoi_unix_conv_errno(errno);
		}

		if (res == SIGPWR) {
			yo_setmask(SIG_SETMASK, &old, NULL);
			return YO_EINTR;
		}

		int sig = yoi_unix_conv_signal(res);
		if (sig < 0) continue;

		*pres = sig;
		yo_setmask(SIG_SETMASK, &old, NULL);
		return YO_OK;
	}
}

yo_code_t yo_getpath(yo_path_type_t type, char **pres) {
	switch (type) {
		case YO_PATH_HOME: {
			char *res = _yoi_generic_getenvpath(NULL);
			if (!res) return yoi_unix_conv_errno(errno);

			*pres = res;
			return YO_OK;
		}
		case YO_PATH_CACHE: {
			char *res = _yoi_unix_getpath("XDG_CACHE_HOME", "/.cache");
			if (!res) return yoi_unix_conv_errno(errno);

			*pres = res;
			return YO_OK;
		}
		case YO_PATH_CONFIG: {
			char *res = _yoi_unix_getpath("XDG_CONFIG_HOME", "/.config");
			if (!res) return yoi_unix_conv_errno(errno);

			*pres = res;
			return YO_OK;
		}
		case YO_PATH_DATA: {
			char *res = _yoi_unix_getpath("XDG_DATA_HOME", "/.local/share");
			if (!res) return yoi_unix_conv_errno(errno);

			*pres = res;
			return YO_OK;
		}
		case YO_PATH_RUNTIME: {
			const char *res;

			const char *env = getenv("XDG_RUNTIME_DIR");
			if (env && *env) res = env;
			else res = "/tmp";

			*pres = malloc(strlen(res) + 1);
			if (!*pres) return yoi_unix_conv_errno(errno);

			strcpy(*pres, res);
			return YO_OK;
		}
		case YO_PATH_CWD: {
			char *buff = malloc(PATH_MAX);
			size_t buffn = PATH_MAX;
			if (!buff) return YO_ENOMEM;

			while (true) {
				errno = 0;
				if (getcwd(buff, buffn)) break;
				if (errno != ERANGE) {
					free(buff);
					return yoi_unix_conv_errno(errno);
				}

				buffn *= 2;
				free(buff);
				buff = malloc(buffn);
				if (!buff) return YO_ENOMEM;
			}

			*pres = realloc(buff, strlen(buff) + 1);
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
		if (unsetenv(name) < 0) return yoi_unix_conv_errno(errno);
	}
	else {
		if (setenv(name, val, true) < 0) return yoi_unix_conv_errno(errno);
	}

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

yo_time_t yo_time(yo_clock_t clock) {
	struct timespec res;
	int err;
	switch (clock) {
		case YO_CLOCK_REALTIME: err = clock_gettime(CLOCK_REALTIME, &res); break;
		case YO_CLOCK_MONOTIME: err = clock_gettime(CLOCK_MONOTONIC, &res); break;
		case YO_CLOCK_CPUTIME: {
			#ifdef YO_USE_LINUX
				err = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &res); break;
			#else
				struct tms buff;
				times(&buff);
				res.tv_sec = (buff.tms_stime + buff.tms_utime) / CLOCKS_PER_SEC;
				res.tv_nsec = ((buff.tms_stime + buff.tms_utime) % CLOCKS_PER_SEC) * (1000000000 / CLOCKS_PER_SEC);
				err = 0;
				break;
			#endif
		}
	}

	assert(err >= 0 && "retrieving clock failed");

	return (yo_time_t) { .sec = res.tv_sec, .nsec = res.tv_nsec };
}
void yo_timesleep(yo_time_t until) {
	struct timespec req = { .tv_sec = until.sec, .tv_nsec = until.nsec };
	while (true) {
		if (nanosleep(&req, NULL) == 0) break;
		if (errno == EINTR) continue;
	}
}
