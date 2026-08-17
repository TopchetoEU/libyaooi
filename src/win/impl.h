#pragma once

#include <stdbool.h>

#include <winsock2.h>
#include <windows.h>
#include <winnt.h>

#include <ev/filelist.h>
#include <ev/io.h>

#include "../fallback/queue.h" // IWYU pragma: export

typedef struct {
	HANDLE hnd;
	bool done;
	WIN32_FIND_DATAW data;
} evi_dir_impl_t;

typedef struct {
	enum {
		EVI_WIN_HND,
		EVI_WIN_SOCK,
	} kind;
	union {
		HANDLE hnd;
		SOCKET sock;
	};
} evi_fd_impl_t;

typedef struct {
} evi_fd_ioq_t;

typedef struct {
	HANDLE hnd;
} evi_proc_impl_t;

typedef struct {
} evi_req_ioq_t;

struct ev_enviter {
	wchar_t *data, *curr;
	char *lastalloc;
};

static void evi_win_mkhnd(ev_filelist_t fl, ev_fd_t res, HANDLE hnd);
static void evi_win_mksock(ev_filelist_t fl, ev_fd_t res, SOCKET sock);

static wchar_t *evi_win_fix_path(wchar_t *path);

static ev_time_t evi_win_conv_filetime(FILETIME filetime);
static ev_code_t evi_win_conv_errno(int winerr);
static int evi_win_conv_addr(ev_addr_t addr, uint16_t port, struct sockaddr_storage *pres);
static void evi_win_conv_sockaddr(struct sockaddr_storage *sockaddr, ev_addr_t *pres, uint16_t *pport);
static wchar_t *evi_win_conv_utf8(const char *str, size_t extra_n);
static char *evi_win_conv_utf16(const wchar_t *wstr);
