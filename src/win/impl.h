// libyaooi, Copyright (C) 2025-2026 topchetoeu, see LICENSE for full LGPL text

#pragma once

#include <stdbool.h>

#include <winsock2.h>
#include <windows.h>
#include <winnt.h>

#include <yaooi/io.h>

#include "../fallback/queue.h" // IWYU pragma: export

struct yo_dir {
	HANDLE hnd;
	bool done;
	WIN32_FIND_DATAW data;
};

struct yo_fd {
	bool owned;

	enum {
		YOI_WIN_HND,
		YOI_WIN_SOCK,
	} kind;
	union {
		HANDLE hnd;
		SOCKET sock;
	};
};
struct yo_proc {
	HANDLE hnd;
};
struct yo_enviter {
	wchar_t *data, *curr;
	char *lastalloc;
};

typedef struct {
} yoi_req_ioq_t;

static void yoi_win_mkhnd(yo_fd_t res, HANDLE hnd);
static void yoi_win_mksock(yo_fd_t res, SOCKET sock);

static wchar_t *yoi_win_fix_path(wchar_t *path);

static yo_time_t yoi_win_conv_filetime(FILETIME filetime);
static yo_code_t yoi_win_conv_errno(int winerr);
static int yoi_win_conv_addr(yo_addr_t addr, uint16_t port, struct sockaddr_storage *pres);
static void yoi_win_conv_sockaddr(struct sockaddr_storage *sockaddr, yo_addr_t *pres, uint16_t *pport);
static wchar_t *yoi_win_conv_utf8(const char *str, size_t extra_n);
static char *yoi_win_conv_utf16(const wchar_t *wstr);
