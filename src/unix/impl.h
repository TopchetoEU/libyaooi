// libyaooi, Copyright (C) 2025-2026 topchetoeu, see LICENSE for full LGPL text

#pragma once

#include <stdint.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>

#include <yaooi/io.h>
#include <yaooi/errno.h>
#include <yaooi/addr.h>
#include <yaooi/queue.h>

#include "./async.h" // IWYU pragma: export

struct yo_fd {
	// Common
	yo_req_t head;
	bool owned;

	#ifndef YO_USE_LINUX
		bool is_at;
	#endif
	union {
		int fd;
		#ifndef YO_USE_LINUX
			char *at;
		#endif
	};

	yoi_fd_ioq_t ioq;
};
struct yo_dir {
	yo_req_t head;
	DIR *dir;
};
struct yo_proc {
	yo_req_t head;
	pid_t pid;
};
struct yo_tty_raw {
	int fd;
	struct termios pryo_mode;
};
struct yo_enviter {
	char **enviter;
};

static bool yoi_unix_isfd(yo_fd_t fd);

static int yoi_unix_conv_open_flags(yo_open_flags_t flags);
static void yoi_unix_conv_stat_mode(int mode, yo_stat_t *dst);
static void yoi_unix_conv_stat(yo_stat_t *dst, struct stat *src);

static int yoi_unix_conv_signal(int sig);
static yo_code_t yoi_unix_conv_errno(int unixerr);
static yo_code_t yoi_unix_conv_aierr(int aierr);
static int yoi_unix_conv_addr(yo_addr_t addr, uint16_t port, struct sockaddr_storage *pres);
static void yoi_unix_conv_sockaddr(struct sockaddr_storage *sockaddr, yo_addr_t *pres, uint16_t *pport);

static int yoi_unix_new_sock(yo_proto_t proto, yo_addr_type_t type);
