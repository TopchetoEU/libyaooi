#pragma once

#include <stdint.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <termios.h>

#include <ev/io.h>
#include <ev/errno.h>
#include <ev/addr.h>

#include "./async.h" // IWYU pragma: export

typedef struct {
	#ifndef EV_USE_LINUX
		bool is_at;
	#endif
	union {
		int fd;
		#ifndef EV_USE_LINUX
			char *at;
		#endif
	};

	evi_async_fd_t async;
} evi_fd_impl_t;
struct evi_dir_impl {
	DIR *dir;
};
struct evi_proc_impl {
	pid_t pid;
};

struct ev_tty_raw {
	int fd;
	struct termios prev_mode;
};
struct ev_enviter {
	char **enviter;
};

static bool evi_unix_isfd(ev_fd_t fd);

static int evi_unix_conv_open_flags(ev_open_flags_t flags);
static void evi_unix_conv_stat_mode(int mode, ev_stat_t *dst);
static void evi_unix_conv_stat(ev_stat_t *dst, struct stat *src);

static int evi_unix_conv_signal(int sig);
static ev_code_t evi_unix_conv_errno(int unixerr);
static ev_code_t evi_unix_conv_aierr(int aierr);
static int evi_unix_conv_addr(ev_addr_t addr, uint16_t port, struct sockaddr_storage *pres);
static void evi_unix_conv_sockaddr(struct sockaddr_storage *sockaddr, ev_addr_t *pres, uint16_t *pport);

static int evi_unix_new_sock(ev_proto_t proto, ev_addr_type_t type);
