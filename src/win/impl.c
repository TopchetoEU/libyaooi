#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <wchar.h>

#include <winerror.h>
#include <winsock2.h>
#include <windows.h>
#include <errhandlingapi.h>
#include <fileapi.h>
#include <handleapi.h>
#include <ws2tcpip.h>
#include <ws2ipdef.h>
#include <shlobj.h>
#include <processenv.h>
#include <processthreadsapi.h>
#include <synchapi.h>

#include <ev/conf.h>
#include <ev/errno.h>
#include <ev/io.h>
#include <ev/filelist.h>
#include <ev/queue.h>

#include "./impl.h" // IWYU pragma: export

#include "../utils/lists.h"
#include "../filelist.c"
#include "../fallback/queue.c" // IWYU pragma: export
#include "../queue.c"
#include "ev/time.h"

// FIXME: never before run code, shat it out in an evening.
// Consider windows as unsupported, until I can be bothered to cross-compile luajit

#define COMBINE64(a, b) (((uint64_t)(b) << 32) | (a))

static bool _evi_win_initialized = false;

static void _evi_win_init() {
	if (_evi_win_initialized) return;
	_evi_win_initialized = true;

	WSADATA data;
	WSAStartup(MAKEWORD(2, 2), &data);
	// ev->in = evi_win_mkhnd(GetStdHandle(STD_INPUT_HANDLE));
	// ev->out = evi_win_mkhnd(GetStdHandle(STD_OUTPUT_HANDLE));
	// ev->err = evi_win_mkhnd(GetStdHandle(STD_ERROR_HANDLE));

	// return EV_OK;
}

static char *_evi_win_getpath(int id, const wchar_t *suffix) {
	wchar_t *buff = suffix ? malloc(sizeof *buff * (MAX_PATH + wcslen(suffix) + 1)) : malloc(sizeof *buff * (MAX_PATH + 1));
	if (!buff) return NULL;
	if (SHGetFolderPathW(NULL, id, NULL, 0, buff) != S_OK) return NULL;

	if (suffix) wcscpy(buff, suffix);

	char *res = evi_win_conv_utf16(buff);
	free(buff);
	return res;
}
static SOCKET _evi_win_sock_new(ev_proto_t proto, ev_addr_type_t type) {
	return socket(
		type == EV_ADDR_IPV4 ? AF_INET : AF_INET6,
		proto == EV_PROTO_UDP ? SOCK_DGRAM : SOCK_STREAM,
		proto == EV_PROTO_UDP ? IPPROTO_UDP : IPPROTO_TCP
	);
}

static int _evi_win_child_std_new(bool in, HANDLE *pparent, HANDLE *pchild, ev_fd_t *pnew) {
	SECURITY_ATTRIBUTES attribs = { .nLength = sizeof attribs, .bInheritHandle = true };

	ev_fd_t res = malloc(sizeof *res);
	if (!res) return -1;

	HANDLE parent, child;

	if (in) {
		if (!CreatePipe(&child, &parent, &attribs, 0)) {
			free(res);
			return -1;
		}
	}
	else {
		if (!CreatePipe(&parent, &child, &attribs, 0)) {
			free(res);
			return -1;
		}
	}

	if (!SetHandleInformation(&parent, HANDLE_FLAG_INHERIT, 0)) {
		CloseHandle(parent);
		CloseHandle(child);
		free(res);
		return -1;
	}

	*pparent = parent;
	*pchild = child;
	*pnew = res;
	return 0;
}
static wchar_t *_evi_win_argv_to_cmdline_raw(const char **argv) {
	size_t n = 0, buff_n = 0;

	for (const char **it = argv; *it; it++) {
		n++;
		buff_n += strlen(*it) + 1;
	}

	wchar_t *buff = malloc(sizeof *buff * buff_n);
	if (!buff) return NULL;

	buff[0] = 0;

	for (size_t i = 0; i < n; i++) {
		if (i != 0) {
			wcscat(buff, L" ");
		}

		wchar_t *warg = evi_win_conv_utf8(argv[i], 0);
		if (warg == NULL) {
			free(buff);
			return NULL;
		}

		wcscat(buff, warg);
	}

	return buff;
}
static wchar_t *_evi_win_argv_to_cmdline(const char **argv, ev_spawn_flags_t flags) {
	// Horray for windows-specific weirdness (I hate this OS with a burning passion)
	if (flags == EV_SPAWN_NOESCAPE) {
		return _evi_win_argv_to_cmdline_raw(argv);
	}

	size_t n = 0, buff_n = 0;

	for (const char **it = argv; *it; it++) {
		n++;
		buff_n += 1 /* " */ + strlen(*it) * 2 /* Assuming each one is \ */ + 1 /* " */ + 1 /* Trailing space / \0 */;
	}

	wchar_t *buff = malloc(sizeof *buff * buff_n);
	if (!buff) return NULL;

	wchar_t *curr = buff;

	for (size_t i = 0; i < n; i++) {
		wchar_t *warg = evi_win_conv_utf8(argv[i], 0);
		if (warg == NULL) {
			free(buff);
			return NULL;
		}

		*(curr++) = '\"';

		size_t back_n = 0;

		for (wchar_t *it = warg; *it; it++) {
			if (*it == '\\') back_n++;
			else if (*it == '\"') {
				for (size_t i = 0; i < back_n; i++) {
					*(curr++) = '\\';
					*(curr++) = '\\';
				}
				*(curr++) = '\\';
				*(curr++) = '\"';
				back_n = 0;
			}
			else {
				for (size_t i = 0; i < back_n; i++) {
					*(curr++) = '\\';
				}
				back_n = 0;
				*(curr++) = *it;
			}
		}

		for (size_t i = 0; i < back_n; i++) {
			*(curr++) = '\\';
			*(curr++) = '\\';
		}

		*(curr++) = '\"';

		if (i == n - 1) *(curr++) = '\0';
		else *(curr++) = ' ';

		free(warg);
	}

	buff = realloc(buff, sizeof *buff * (curr - buff));
	return buff;
}
static wchar_t *_evi_win_envp_to_envblock(const char **envp) {
	if (!envp) return NULL;

	size_t n = 0, buff_n = 0;

	for (const char **it = envp; *it; it++) {
		n++;
		buff_n += strlen(*it) + 1 /* terminating \0 */;
	}

	wchar_t *buff = malloc(sizeof *buff * (buff_n + 1 /* terminating \0 */));
	wchar_t *curr = buff;

	for (size_t i = 0; i < n; i++) {
		int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, envp[i], -1, curr, buff_n - (curr - buff));
		if (!n) {
			free(buff);
			return NULL;
		}

		curr += n;
	}

	*curr = '\0';
	buff = realloc(buff, sizeof *buff * (curr - buff + 1));
	return buff;
}

static void evi_win_mkhnd(ev_filelist_t fl, ev_fd_t res, HANDLE hnd) {
	res->owned = true;
	res->impl.kind = EVI_WIN_HND;
	res->impl.hnd = hnd;

	evi_dlist_add(fl, fl->fd_head, res);
}
static void evi_win_mksock(ev_filelist_t fl, ev_fd_t res, SOCKET sock) {
	res->owned = true;
	res->impl.kind = EVI_WIN_SOCK;
	res->impl.sock = sock;

	evi_dlist_add(fl, fl->fd_head, res);
}

static wchar_t *evi_win_fix_path(wchar_t *path) {
	for (wchar_t *it = wcschr(path, '/'); it; it = wcschr(path, '/')) {
		*it = '\\';
	}

	return path;
}
// Everybody uses utf8, but NOOOOO, windows just HAD to use utf16
static wchar_t *evi_win_conv_utf8(const char *str, size_t extra_n) {
	int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, -1, NULL, 0);
	if (len == 0) return NULL;

	wchar_t *wstr = malloc(sizeof *wstr * (len + extra_n));
	if (!wstr) {
		SetLastError(ERROR_OUTOFMEMORY);
		return NULL;
	}

	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, -1, wstr, len);
	return wstr;
}
static char *evi_win_conv_utf16(const wchar_t *wstr) {
	int len = WideCharToMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS, wstr, -1, NULL, 0, NULL, NULL);
	if (len == 0) return NULL;

	char *str = malloc(len);
	if (!str) {
		SetLastError(ERROR_OUTOFMEMORY);
		return NULL;
	}

	WideCharToMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS, wstr, -1, str, len, NULL, NULL);
	return str;
}

static ev_time_t evi_win_conv_filetime(FILETIME filetime) {
	static const uint64_t EPOCH_DIFFERENCE = 11644473600;

	uint64_t total_ticks = ((uint64_t)filetime.dwHighDateTime << 32) | (uint64_t)filetime.dwLowDateTime;

	return (ev_time_t) {
		.sec = (time_t)(total_ticks / 100000000) - EPOCH_DIFFERENCE,
		.nsec = (long)(total_ticks % 100000000) * 100,
	};
}
static ev_code_t evi_win_conv_errno(int winerr) {
	switch (winerr) {
		case ERROR_ACCESS_DENIED: return EV_EPERM;
		case ERROR_ACTIVE_CONNECTIONS: return EV_EAGAIN;
		case ERROR_ADDRESS_ALREADY_ASSOCIATED:  return EV_EADDRINUSE;
		case ERROR_ALREADY_EXISTS: return EV_EEXIST;
		case ERROR_BAD_DEVICE: return EV_ENODEV;
		case ERROR_BAD_EXE_FORMAT: return EV_ENOEXEC;
		case ERROR_BAD_NET_NAME: return EV_ENOENT;
		case ERROR_BAD_NET_RESP: return EV_ENOSYS;
		case ERROR_BAD_NETPATH: return EV_ENOENT;
		case ERROR_BAD_PATHNAME: return EV_ENOENT;
		case ERROR_BAD_PIPE: return EV_EPIPE;
		case ERROR_BAD_UNIT: return EV_ENODEV;
		case ERROR_BAD_USERNAME: return EV_EINVAL;
		case ERROR_BEGINNING_OF_MEDIA: return EV_EIO;
		// case ERROR_BROKEN_PIPE: return EV_EOF;
		case ERROR_BROKEN_PIPE: return EV_EPIPE;
		case ERROR_BUFFER_OVERFLOW: return EV_EFAULT;
		case ERROR_BUS_RESET: return EV_EIO;
		case ERROR_BUSY: return EV_EBUSY;
		case ERROR_CALL_NOT_IMPLEMENTED: return EV_ENOSYS;
		case ERROR_CANCELLED: return EV_EINTR;
		case ERROR_CANNOT_MAKE: return EV_EPERM;
		case ERROR_CANT_RESOLVE_FILENAME: return EV_ELOOP;
		case ERROR_CHILD_NOT_COMPLETE: return EV_EBUSY;
		case ERROR_COMMITMENT_LIMIT: return EV_EAGAIN;
		case ERROR_CONNECTION_ABORTED: return EV_ECONNABORTED;
		case ERROR_CONNECTION_REFUSED: return EV_ECONNREFUSED;
		case ERROR_CRC: return EV_EIO;
		case ERROR_DEV_NOT_EXIST: return EV_ENOENT;
		case ERROR_DEVICE_DOOR_OPEN: return EV_EIO;
		case ERROR_DEVICE_IN_USE: return EV_EAGAIN;
		case ERROR_DEVICE_REQUIRES_CLEANING: return EV_EIO;
		case ERROR_DIR_NOT_EMPTY: return EV_ENOTEMPTY;
		case ERROR_DIRECTORY: return EV_ENOTDIR;
		case ERROR_DISK_CORRUPT: return EV_EIO;
		case ERROR_DISK_FULL: return EV_ENOSPC;
		case ERROR_DS_GENERIC_ERROR: return EV_EIO;
		case ERROR_DUP_NAME: return EV_ENOTUNIQ;
		case ERROR_EA_LIST_INCONSISTENT: return EV_EINVAL;
		case ERROR_EA_TABLE_FULL: return EV_ENOSPC;
		case ERROR_EAS_DIDNT_FIT: return EV_ENOSPC;
		case ERROR_EAS_NOT_SUPPORTED: return EV_ENOTSUP;
		case ERROR_END_OF_MEDIA: return EV_ENOSPC;
		case ERROR_EOM_OVERFLOW: return EV_EIO;
		case ERROR_EXE_MACHINE_TYPE_MISMATCH: return EV_ENOEXEC;
		case ERROR_EXE_MARKED_INVALID: return EV_ENOEXEC;
		case ERROR_FILE_CORRUPT: return EV_EEXIST;
		case ERROR_FILE_EXISTS: return EV_EEXIST;
		case ERROR_FILE_INVALID: return EV_ENXIO;
		case ERROR_FILE_NOT_FOUND: return EV_ENOENT;
		case ERROR_FILEMARK_DETECTED: return EV_EIO;
		case ERROR_FILENAME_EXCED_RANGE: return EV_ENAMETOOLONG;
		case ERROR_GEN_FAILURE: return EV_EIO;
		case ERROR_HANDLE_DISK_FULL: return EV_ENOSPC;
		case ERROR_HANDLE_EOF: return EV_ENODATA;
		case ERROR_HOST_UNREACHABLE: return EV_EHOSTUNREACH;
		case ERROR_INSUFFICIENT_BUFFER: return EV_EINVAL;
		case ERROR_INVALID_ADDRESS: return EV_EINVAL;
		case ERROR_INVALID_AT_INTERRUPT_TIME: return EV_EINTR;
		case ERROR_INVALID_BLOCK_LENGTH: return EV_EIO;
		case ERROR_INVALID_DATA: return EV_EINVAL;
		case ERROR_INVALID_DRIVE: return EV_ENODEV;
		case ERROR_INVALID_EA_NAME: return EV_EINVAL;
		case ERROR_INVALID_EXE_SIGNATURE: return EV_ENOEXEC;
		case ERROR_INVALID_FLAGS: return EV_EBADF;
		case ERROR_INVALID_FUNCTION: return EV_EISDIR;
		// case ERROR_INVALID_FUNCTION: return EV_EINVAL;
		case ERROR_INVALID_HANDLE: return EV_EBADF;
		case ERROR_INVALID_NAME: return EV_ENOENT;
		case ERROR_INVALID_PARAMETER: return EV_EINVAL;
		case ERROR_INVALID_REPARSE_DATA: return EV_ENOENT;
		case ERROR_INVALID_SIGNAL_NUMBER: return EV_EINVAL;
		case ERROR_IO_DEVICE: return EV_EIO;
		case ERROR_IO_INCOMPLETE: return EV_EAGAIN;
		case ERROR_IO_PENDING: return EV_EAGAIN;
		case ERROR_IOPL_NOT_ENABLED: return EV_ENOEXEC;
		case ERROR_LOCK_VIOLATION: return EV_EBUSY;
		case ERROR_MAX_THRDS_REACHED: return EV_EAGAIN;
		case ERROR_META_EXPANSION_TOO_LONG: return EV_EINVAL;
		case ERROR_MOD_NOT_FOUND: return EV_ENOENT;
		case ERROR_MORE_DATA: return EV_EMSGSIZE;
		case ERROR_NEGATIVE_SEEK: return EV_EINVAL;
		case ERROR_NETNAME_DELETED: return EV_ECONNRESET;
		case ERROR_NETWORK_UNREACHABLE: return EV_ENETUNREACH;
		case ERROR_NO_DATA_DETECTED: return EV_EIO;
		case ERROR_NO_DATA: return EV_EPIPE;
		case ERROR_NO_MEDIA_IN_DRIVE: return EV_ENOMEDIUM;
		case ERROR_NO_MORE_FILES: return EV_ENOENT;
		case ERROR_NO_MORE_ITEMS: return EV_ENOENT;
		case ERROR_NO_MORE_SEARCH_HANDLES: return EV_ENFILE;
		case ERROR_NO_PROC_SLOTS: return EV_EAGAIN;
		case ERROR_NO_SIGNAL_SENT: return EV_EIO;
		case ERROR_NO_SYSTEM_RESOURCES: return EV_EFBIG;
		case ERROR_NO_TOKEN: return EV_EINVAL;
		case ERROR_NO_UNICODE_TRANSLATION: return EV_ECHARSET;
		case ERROR_NOACCESS: return EV_EACCES;
		case ERROR_NONE_MAPPED: return EV_EINVAL;
		case ERROR_NONPAGED_SYSTEM_RESOURCES: return EV_EAGAIN;
		case ERROR_NOT_CONNECTED: return EV_ENOTCONN;
		case ERROR_NOT_ENOUGH_MEMORY: return EV_ENOMEM;
		case ERROR_NOT_ENOUGH_QUOTA: return EV_EIO;
		case ERROR_NOT_OWNER: return EV_EPERM;
		case ERROR_NOT_READY: return EV_ENOMEDIUM;
		case ERROR_NOT_SAME_DEVICE: return EV_EXDEV;
		case ERROR_NOT_SUPPORTED: return EV_ENOTSUP;
		case ERROR_OPEN_FAILED: return EV_EIO;
		case ERROR_OPEN_FILES: return EV_EAGAIN;
		case ERROR_OPERATION_ABORTED: return EV_ECANCELED;
		case ERROR_OUTOFMEMORY: return EV_ENOMEM;
		case ERROR_PAGED_SYSTEM_RESOURCES: return EV_EAGAIN;
		case ERROR_PAGEFILE_QUOTA: return EV_EAGAIN;
		case ERROR_PATH_NOT_FOUND: return EV_ENOENT;
		case ERROR_PIPE_BUSY: return EV_EBUSY;
		case ERROR_PIPE_CONNECTED: return EV_EBUSY;
		case ERROR_PIPE_LISTENING: return EV_ECOMM;
		case ERROR_PIPE_NOT_CONNECTED: return EV_EPIPE;
		case ERROR_POSSIBLE_DEADLOCK: return EV_EDEADLK;
		case ERROR_PRIVILEGE_NOT_HELD: return EV_EPERM;
		case ERROR_PROC_NOT_FOUND: return EV_ESRCH;
		case ERROR_PROCESS_ABORTED: return EV_EFAULT;
		case ERROR_REM_NOT_LIST: return EV_ENOENT;
		case ERROR_SECTOR_NOT_FOUND: return EV_EINVAL;
		case ERROR_SEEK: return EV_ESPIPE;
		case ERROR_SEM_TIMEOUT: return EV_ETIMEDOUT;
		case ERROR_SERVICE_REQUEST_TIMEOUT: return EV_ETIMEDOUT;
		case ERROR_SETMARK_DETECTED: return EV_EIO;
		case ERROR_SHARING_BUFFER_EXCEEDED: return EV_ENOLCK;
		case ERROR_SHARING_VIOLATION: return EV_EBUSY;
		case ERROR_SIGNAL_PENDING: return EV_EBUSY;
		case ERROR_SIGNAL_REFUSED: return EV_EIO;
		case ERROR_SXS_CANT_GEN_ACTCTX: return EV_ELIBBAD;
		case ERROR_SYMLINK_NOT_SUPPORTED: return EV_EINVAL;
		case ERROR_THREAD_1_INACTIVE: return EV_EINVAL;
		case ERROR_TIMEOUT: return EV_EBUSY;
		case ERROR_TOO_MANY_LINKS: return EV_EMLINK;
		case ERROR_TOO_MANY_OPEN_FILES: return EV_EMFILE;
		case ERROR_UNEXP_NET_ERR: return EV_EIO;
		case ERROR_WAIT_NO_CHILDREN: return EV_ECHILD;
		case ERROR_WORKING_SET_QUOTA: return EV_EAGAIN;
		case ERROR_WRITE_PROTECT: return EV_EROFS;
		case ERROR_RESOURCE_DATA_NOT_FOUND: return EV_ENOEXEC;
		case ERROR_RESOURCE_TYPE_NOT_FOUND: return EV_ENOEXEC;
		case ERROR_RESOURCE_NAME_NOT_FOUND: return EV_ENOEXEC;
		case ERROR_RESOURCE_LANG_NOT_FOUND: return EV_ENOEXEC;

		case WSAEACCES: return EV_EACCES;
		case WSAEADDRINUSE: return EV_EADDRINUSE;
		case WSAEADDRNOTAVAIL: return EV_EADDRNOTAVAIL;
		case WSAEAFNOSUPPORT: return EV_EAFNOSUPPORT;
		case WSAEALREADY: return EV_EALREADY;
		case WSAECONNABORTED: return EV_ECONNABORTED;
		case WSAECONNREFUSED: return EV_ECONNREFUSED;
		case WSAECONNRESET: return EV_ECONNRESET;
		case WSAEFAULT: return EV_EFAULT;
		case WSAEHOSTUNREACH: return EV_EHOSTUNREACH;
		case WSAEINTR: return EV_EINTR;
		case WSAEINVAL: return EV_EINVAL;
		case WSAEISCONN: return EV_EISCONN;
		case WSAEMFILE: return EV_EMFILE;
		case WSAEMSGSIZE: return EV_EMSGSIZE;
		case WSAENETUNREACH: return EV_ENETUNREACH;
		case WSAENOBUFS: return EV_ENOBUFS;
		case WSAENOTCONN: return EV_ENOTCONN;
		case WSAENOTSOCK: return EV_ENOTSOCK;
		case WSAEPFNOSUPPORT: return EV_EPFNOSUPPORT;
		case WSAEPROTONOSUPPORT: return EV_EPROTONOSUPPORT;
		case WSAESHUTDOWN: return EV_EPIPE;
		case WSAESOCKTNOSUPPORT: return EV_ESOCKTNOSUPPORT;
		case WSAETIMEDOUT: return EV_ETIMEDOUT;
		case WSAEWOULDBLOCK: return EV_EAGAIN;
		case WSAHOST_NOT_FOUND: return EV_ENOENT;
		case WSANO_DATA: return EV_ENOENT;

		default: return EV_EUNKNOWN;
	}
}

static int evi_win_conv_addr(ev_addr_t addr, uint16_t port, struct sockaddr_storage *pres) {
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
		SOCKADDR_IN6 res;
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
static void evi_win_conv_sockaddr(struct sockaddr_storage *sockaddr, ev_addr_t *pres, uint16_t *pport) {
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

		// for (size_t i = 0; i < 8; i++) {
		// 	pres->v6[i] = ntohs(pres->v6[i]);
		// }
	}
}

ev_code_t ev_fd_new(ev_filelist_t fl, ev_fd_t *pres, uint64_t fd, bool owned) {
	ev_fd_t res = malloc(sizeof *res);
	if (!res) return EV_ENOMEM;

	evi_win_mkhnd(fl, res, (HANDLE)fd);
	res->owned = owned;

	*pres = res;
	return EV_OK;
}
void ev_fd_close(ev_fd_t fd) {
	if (fd->owned) {
		switch (fd->impl.kind) {
			case EVI_WIN_HND: CloseHandle(fd->impl.hnd); break;
			case EVI_WIN_SOCK: closesocket(fd->impl.sock); break;
		}
	}

	evi_dlist_del(fl, fd);
	free(fd);
}

ev_code_t ev_read(ev_fd_t fd, char *buff, size_t *pn) {
	switch (fd->impl.kind) {
		case EVI_WIN_HND: {
			DWORD out_n;

			if (!ReadFile(fd->impl.hnd, (void*)buff, *pn, &out_n, NULL)) {
				if (GetLastError() == ERROR_HANDLE_EOF || GetLastError() == ERROR_BROKEN_PIPE) {
					*pn = 0;
					return EV_OK;
				}

				return evi_win_conv_errno(GetLastError());
			}
			*pn = out_n;
			return EV_OK;
		}
		case EVI_WIN_SOCK: {
			int res = recv(fd->impl.sock, (void*)buff, *pn, 0);
			if (res < 0) return evi_win_conv_errno(WSAGetLastError());

			*pn = res;
			return EV_OK;
		}
		default: return EV_EBADF;
	}
}
ev_code_t ev_write(ev_fd_t fd, char *buff, size_t *pn) {
	switch (fd->impl.kind) {
		case EVI_WIN_HND: {
			DWORD out_n;

			if (!WriteFile(fd->impl.hnd, (void*)buff, *pn, &out_n, NULL)) {
				if (GetLastError() == ERROR_HANDLE_EOF || GetLastError() == ERROR_BROKEN_PIPE) {
					*pn = 0;
					return EV_OK;
				}

				return evi_win_conv_errno(GetLastError());
			}
			*pn = out_n;
			return EV_OK;
		}
		case EVI_WIN_SOCK: {
			int res = send(fd->impl.sock, (void*)buff, *pn, 0);
			if (res < 0) return evi_win_conv_errno(WSAGetLastError());

			*pn = res;
			return EV_OK;
		}
		default: return EV_EBADF;
	}
}
ev_code_t ev_sync(ev_fd_t fd) {
	if (fd->impl.kind != EVI_WIN_HND) return EV_EBADF;
	if (!FlushFileBuffers(fd->impl.hnd)) return evi_win_conv_errno(GetLastError());
	return EV_OK;
}
ev_code_t ev_stat(ev_fd_t fd, ev_stat_t *buff) {
	if (fd->impl.kind != EVI_WIN_HND) return EV_EBADF;

	BY_HANDLE_FILE_INFORMATION info;
	if (!GetFileInformationByHandle(fd->impl.hnd, &info)) return evi_win_conv_errno(GetLastError());

	// Fake it till we make it .-.

	if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
		buff->mode = 0555;
	}
	else {
		buff->mode = 0777;
	}

	if (info.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) {
		buff->type = EV_STAT_REG;
	}
	else if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		buff->type = EV_STAT_DIR;
	}
	else if (info.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) {
		buff->type = EV_STAT_BLK;
	}
	else if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
		buff->type = EV_STAT_LINK;
		buff->mode = 0777;
	}

	buff->uid = 1000;
	buff->gid = 1000;

	buff->atime = evi_win_conv_filetime(info.ftLastAccessTime);
	buff->mtime = evi_win_conv_filetime(info.ftLastWriteTime);
	// The semantics of this are dubious, do we equate the creation of a file to the last change of its metadata
	// TODO: look into this further
	buff->ctime = evi_win_conv_filetime(info.ftCreationTime);

	buff->size = COMBINE64(info.nFileSizeLow, info.nFileSizeHigh);
	// TODO: does windows expose a preferred blk size somehow? For now, we pick a reasonable arbitrary blksize
	buff->blksize = 4096;

	buff->inode = COMBINE64(info.nFileIndexLow, info.nFileIndexHigh);
	buff->links = info.nNumberOfLinks;

	return EV_OK;
}

ev_code_t ev_file_open(ev_filelist_t fl, ev_fd_t *pres, const char *path, ev_open_flags_t flags, int mode) {
	(void)mode;

	ev_fd_t res = malloc(sizeof *res);
	if (!res) return EV_ENOMEM;

	DWORD access = 0;
	DWORD access_others = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	DWORD create_mode = 0;
	DWORD res_flags = 0;
	SECURITY_ATTRIBUTES sec_attribs = { 0 };
	sec_attribs.nLength = sizeof sec_attribs;

	if (!(flags & EV_OPEN_STAT)) {
		if (flags & EV_OPEN_APPEND) {
			flags |= EV_OPEN_WRITE;
		}

		if (flags & EV_OPEN_READ) {
			access |= FILE_GENERIC_READ;
			access_others &= ~(FILE_SHARE_DELETE);
		}
		if (flags & EV_OPEN_WRITE) {
			access |= FILE_GENERIC_WRITE;
			access_others &= ~(FILE_SHARE_DELETE | FILE_SHARE_WRITE);

			if (!(flags & EV_OPEN_APPEND)) {
				access &= ~FILE_APPEND_DATA;
			}
		}
	}

	switch (flags & (EV_OPEN_CREATE | EV_OPEN_TRUNC)) {
		case 0: create_mode = OPEN_EXISTING; break;
		case EV_OPEN_CREATE: create_mode = OPEN_ALWAYS; break;
		case EV_OPEN_TRUNC: create_mode = TRUNCATE_EXISTING; break;
		case EV_OPEN_CREATE | EV_OPEN_TRUNC: create_mode = CREATE_ALWAYS; break;
	}

	if (flags & EV_OPEN_DIRECT) {
		res_flags |= FILE_FLAG_WRITE_THROUGH;
	}

	if (flags & EV_OPEN_SHARED) {
		sec_attribs.bInheritHandle = true;
	}

	wchar_t *wpath = evi_win_conv_utf8(path, 0);
	if (!wpath) {
		free(res);
		return evi_win_conv_errno(GetLastError());
	}

	HANDLE hnd = CreateFileW(wpath, access, access_others, NULL, create_mode, FILE_ATTRIBUTE_NORMAL | res_flags, NULL);
	free(wpath);
	if (hnd == INVALID_HANDLE_VALUE) {
		free(res);
		return evi_win_conv_errno(GetLastError());
	}

	evi_win_mkhnd(fl, res, hnd);

	*pres = res;
	return EV_OK;
}
ev_code_t ev_file_read(ev_fd_t fd, char *buff, size_t *n, size_t offset) {
	if (fd->impl.kind != EVI_WIN_HND) return EV_EBADF;

	DWORD out_n;
	OVERLAPPED overlapped = { .Pointer = (void*)offset };

	if (!ReadFile(fd->impl.hnd, (void*)buff, *n, &out_n, &overlapped)) {
		if (GetLastError() == ERROR_HANDLE_EOF || GetLastError() == ERROR_BROKEN_PIPE) {
			*n = 0;
			return EV_OK;
		}

		return evi_win_conv_errno(GetLastError());
	}
	*n = out_n;
	return EV_OK;
}
ev_code_t ev_file_write(ev_fd_t fd, char *buff, size_t *n, size_t offset) {
	if (fd->impl.kind != EVI_WIN_HND) return EV_EBADF;

	DWORD out_n;
	OVERLAPPED overlapped = { .Pointer = (void*)offset };

	if (!WriteFile(fd->impl.hnd, buff, *n, &out_n, &overlapped)) {
		if (GetLastError() == ERROR_HANDLE_EOF) {
			*n = 0;
			return EV_OK;
		}

		return evi_win_conv_errno(GetLastError());
	}
	*n = out_n;
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

ev_code_t ev_file_symlink(const char *path, const char *target) {
	wchar_t *wpath = evi_win_conv_utf8(path, 0);
	if (!wpath) return evi_win_conv_errno(GetLastError());

	wchar_t *wtarget = evi_win_conv_utf8(target, 0);
	if (!wpath) {
		free(wpath);
		return evi_win_conv_errno(GetLastError());
	}

	// TODO: handle directories
	bool res = CreateSymbolicLinkW(wtarget, wpath, 0);
	free(wpath);
	free(wtarget);

	if (!res) return evi_win_conv_errno(GetLastError());
	return EV_OK;
}
ev_code_t ev_file_hardlink(const char *path, const char *target) {
	wchar_t *wpath = evi_win_conv_utf8(path, 0);
	if (!wpath) return evi_win_conv_errno(GetLastError());

	wchar_t *wtarget = evi_win_conv_utf8(target, 0);
	if (!wpath) {
		free(wpath);
		return evi_win_conv_errno(GetLastError());
	}

	// TODO: handle directories
	bool res = CreateHardLinkW(wtarget, wpath, 0);
	free(wpath);
	free(wtarget);

	if (!res) return evi_win_conv_errno(GetLastError());
	return EV_OK;
}
ev_code_t ev_file_readlink(const char *path, char **pres) {
	(void)path, (void)pres;
	// Tough luck
	return EV_ENOTSUP;
}
ev_code_t ev_file_remove(const char *path) {
	wchar_t *wpath = evi_win_conv_utf8(path, 0);
	if (!wpath) return evi_win_conv_errno(GetLastError());

	if (!DeleteFileW(wpath)) {
		if (GetLastError() != ERROR_ACCESS_DENIED) {
			free(wpath);
			return evi_win_conv_errno(GetLastError());
		}
	}
	if (!RemoveDirectoryW(wpath)) {
		free(wpath);
		return evi_win_conv_errno(GetLastError());
	}

	free(wpath);
	return EV_OK;
}

ev_code_t ev_dir_new(const char *path, int mode) {
	(void)mode;

	wchar_t *wpath = evi_win_conv_utf8(path, 0);
	if (!wpath) return evi_win_conv_errno(GetLastError());

	bool res = CreateDirectoryW(wpath, NULL);
	free(wpath);
	if (!res) return evi_win_conv_errno(GetLastError());

	return EV_OK;
}
ev_code_t ev_dir_open(ev_filelist_t fl, ev_dir_t *pres, const char *path) {
	wchar_t *wpattern = evi_win_conv_utf8(path, 2);
	if (!wpattern) return evi_win_conv_errno(GetLastError());
	wcscat(wpattern, L"\\*");
	evi_win_fix_path(wpattern);

	WIN32_FIND_DATAW data;
	memset(&data, 0, sizeof data);
	HANDLE hnd = FindFirstFileW(wpattern, &data);
	free(wpattern);
	if (hnd == INVALID_HANDLE_VALUE) return evi_win_conv_errno(GetLastError());

	ev_dir_t res = malloc(sizeof *res);
	if (!res) return EV_ENOMEM;

	res->impl.data = data;
	res->impl.hnd = hnd;
	res->impl.done = false;

	evi_dlist_add(fl, fl->dir_head, res);

	*pres = res;
	return EV_OK;
}
ev_code_t ev_dir_next(ev_dir_t dir, char **pname) {
	while (true) {
		if (dir->impl.done) {
			*pname = NULL;
			return EV_OK;
		}

		bool is_synth = !wcscmp(dir->impl.data.cFileName, L".") || !wcscmp(dir->impl.data.cFileName, L"..");
		if (!is_synth) *pname = evi_win_conv_utf16(dir->impl.data.cFileName);

		if (!FindNextFileW(dir->impl.hnd, &dir->impl.data)) {
			if (GetLastError() == ERROR_NO_MORE_FILES) {
				dir->impl.done = true;
			}
			else {
				return evi_win_conv_errno(GetLastError());
			}
		}

		if (!is_synth) return EV_OK;
	}
}
void ev_dir_close(ev_dir_t dir) {
	FindClose(dir->impl.hnd);
	evi_dlist_del(fl, dir);
	free(dir);
}

ev_code_t ev_socket_bind(ev_filelist_t fl, ev_fd_t *pres, ev_proto_t proto, ev_addr_t addr, uint16_t port, size_t max_n) {
	_evi_win_init();

	ev_fd_t res = malloc(sizeof *res);
	if (!res) return EV_ENOMEM;

	SOCKET sock = _evi_win_sock_new(proto, addr.type);
	if (sock == INVALID_SOCKET) {
		free(res);
		return evi_win_conv_errno(WSAGetLastError());
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*)&(int) { 1 }, sizeof(int)) < 0) {
		free(res);
		closesocket(sock);
		return evi_win_conv_errno(WSAGetLastError());
	}

	struct sockaddr_storage arg_addr;
	int len = evi_win_conv_addr(addr, port, &arg_addr);

	if (bind(sock, (void*)&arg_addr, len) < 0) {
		free(res);
		closesocket(sock);
		return evi_win_conv_errno(WSAGetLastError());
	}
	if (listen(sock, max_n) < 0) {
		free(res);
		closesocket(sock);
		return evi_win_conv_errno(WSAGetLastError());
	}

	evi_win_mksock(fl, res, sock);

	*pres = res;
	return EV_OK;
}
ev_code_t ev_socket_accept(ev_filelist_t fl, ev_fd_t server, ev_fd_t *pres, ev_addr_t *paddr, uint16_t *pport) {
	_evi_win_init();

	ev_fd_t res = malloc(sizeof *res);
	if (!res) return EV_ENOMEM;

	struct sockaddr_storage addr = {};
	socklen_t addr_len = sizeof addr;

	SOCKET client = accept((SOCKET)(size_t)server, (void*)&addr, &addr_len);
	if (!client) {
		free(res);
		return evi_win_conv_errno(WSAGetLastError());
	}

	evi_win_conv_sockaddr(&addr, paddr, pport);
	evi_win_mksock(fl, res, client);

	*pres = (void*)(size_t)client;
	return EV_OK;
}
ev_code_t ev_socket_connect(ev_filelist_t fl, ev_fd_t *pres, ev_proto_t proto, ev_addr_t addr, uint16_t port) {
	_evi_win_init();

	ev_fd_t res = malloc(sizeof *res);
	if (!res) return EV_ENOMEM;

	SOCKET sock = _evi_win_sock_new(proto, addr.type);
	if (sock == INVALID_SOCKET) {
		free(res);
		return evi_win_conv_errno(WSAGetLastError());
	}

	struct sockaddr_storage arg_addr;
	int len = evi_win_conv_addr(addr, port, &arg_addr);

	if (connect(sock, (void*)&arg_addr, len) < 0) {
		closesocket(sock);
		free(res);
		return evi_win_conv_errno(WSAGetLastError());
	}

	evi_win_mksock(fl, res, sock);

	*pres = res;
	return EV_OK;
}

ev_code_t ev_proc_spawn(
	ev_filelist_t fl, ev_proc_t *pres, ev_spawn_flags_t flags,
	const char **argv, const char **envp, const char *cwd,
	ev_fd_t *pin,
	ev_fd_t *pout,
	ev_fd_t *perr
) {
	HANDLE in_parent = NULL, in_child = NULL;
	HANDLE out_parent = NULL, out_child = NULL;
	HANDLE err_parent = NULL, err_child = NULL;

	ev_fd_t in_res = NULL, out_res = NULL, err_res = NULL;

	if (pin) {
		if (_evi_win_child_std_new(true, &in_parent, &in_child, &in_res) < 0) goto err;
	}
	if (pout) {
		if (_evi_win_child_std_new(false, &out_parent, &out_child, &out_res) < 0) goto err_in_pipe;
	}
	if (perr) {
		if (_evi_win_child_std_new(false, &err_parent, &err_child, &err_res) < 0) goto err_out_pipe;
	}

	wchar_t *cmdline = _evi_win_argv_to_cmdline(argv, flags);
	if (!cmdline) goto err_err_pipe;

	wchar_t *envblock = _evi_win_envp_to_envblock(envp);
	if (!envblock) goto err_cmdline;

	wchar_t *procname = evi_win_conv_utf8(argv[0], 0);
	if (!procname) goto err_envblock;

	wchar_t *wcwd = evi_win_conv_utf8(cwd, 0);
	if (cwd && !wcwd) goto err_procname;

	STARTUPINFOW start_info = { .cb = sizeof start_info };
	start_info.hStdInput  = in_child;
	start_info.hStdOutput = out_child;
	start_info.hStdError  = err_child;
	start_info.dwFlags |= STARTF_USESTDHANDLES;

	PROCESS_INFORMATION proc_info;
	BOOL res = CreateProcessW(procname, cmdline, NULL, NULL, true, 0, envblock, wcwd, &start_info, &proc_info);
	if (!res || !proc_info.hProcess) goto err_wcwd;

	free(cmdline);
	free(envblock);
	free(procname);
	free(wcwd);


	if (in_child) CloseHandle(in_child);
	if (out_child) CloseHandle(out_child);
	if (err_child) CloseHandle(err_child);

	if (pin) {
		evi_win_mkhnd(fl, in_res, in_parent);
		*pin = in_res;
	}
	if (pout) {
		evi_win_mkhnd(fl, out_res, out_parent);
		*pout = out_res;
	}
	if (perr) {
		evi_win_mkhnd(fl, err_res, err_parent);
		*perr = err_res;
	}

	*pres = proc_info.hProcess;
	CloseHandle(proc_info.hThread);

	return EV_OK;
err_wcwd:
	free(wcwd);
err_procname:
	free(procname);
err_envblock:
	free(envblock);
err_cmdline:
	free(cmdline);
err_err_pipe:
	if (err_parent) CloseHandle(err_parent);
	if (err_child) CloseHandle(err_child);
err_out_pipe:
	if (out_parent) CloseHandle(out_parent);
	if (out_child) CloseHandle(out_child);
err_in_pipe:
	if (in_parent) CloseHandle(in_parent);
	if (in_child) CloseHandle(in_child);
err:
	return evi_win_conv_errno(GetLastError());
}
ev_code_t ev_proc_wait(ev_proc_t proc, int *psig, int *pcode) {
	switch (WaitForSingleObject(proc->impl.hnd, INFINITE)) {
		case WAIT_ABANDONED:
			return EV_EDEADLK;
		case WAIT_OBJECT_0:
			break;
		case WAIT_TIMEOUT:
			return EV_ETIMEDOUT;
		case WAIT_FAILED:
			return evi_win_conv_errno(GetLastError());
	}

	DWORD code;
	if (!GetExitCodeProcess(proc->impl.hnd, &code)) return evi_win_conv_errno(GetLastError());

	CloseHandle(proc->impl.hnd);

	*psig = -1;
	*pcode = code;

	return EV_OK;
}

ev_code_t ev_dns_getaddrinfo(ev_addrinfo_t *pres, const char *name, ev_addrinfo_flags_t flags) {
	ADDRINFOW hints = { 0 };

	if (flags & EV_AI_IPV4_MAPPED) hints.ai_flags |= EV_AI_IPV4_MAPPED;

	if (flags & EV_AI_IPV6) hints.ai_family = AF_INET6;
	else if (flags & EV_AI_IPV4) hints.ai_family = AF_INET;
	else hints.ai_family = AF_UNSPEC;

	if (flags & EV_AI_BIND) hints.ai_flags |= AI_PASSIVE;
	if (flags & EV_AI_NODNS) hints.ai_flags |= AI_NUMERICHOST;

	ADDRINFOW *list = NULL;

	wchar_t *wname = evi_win_conv_utf8(name, 0);
	if (!wname) return evi_win_conv_errno(GetLastError());

	int code;

	// We still want to resolve a valid loopback IP, even if getaddrinfo
	if (name == NULL) code = GetAddrInfoW(wname, L"80", &hints, &list);
	else code = GetAddrInfoW(wname, L"", &hints, &list);

	free(wname);

	switch (code) {
		case 0: break;
		case EAI_NODATA: break;
		case EAI_NONAME: break;
		case EAI_SOCKTYPE: return EV_EINVAL;
		case EAI_BADFLAGS: return EV_EINVAL;
		case EAI_FAMILY: return EV_ENOTSUP;
		case EAI_MEMORY: return EV_ENOMEM;
		case EAI_AGAIN: return EV_EAGAIN;
		case EAI_FAIL: return EV_EIO;
	}

	size_t n = 0;
	for (ADDRINFOW *it = list; it; it = it->ai_next) n++;

	ev_addrinfo_t res = malloc(sizeof *res + sizeof *res->addr * n);
	if (!res) return EV_ENOMEM;

	size_t i = 0;
	for (ADDRINFOW *it = list; it; it = it->ai_next) {
		uint16_t port;
		ev_addr_t addr;
		evi_win_conv_sockaddr((void*)it->ai_addr, &addr, &port);

		bool found = false;

		for (size_t j = 0; j < i; j++) {
			if (!memcmp(&addr, &res->addr[j], sizeof addr)) {
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

	if (list) FreeAddrInfoW(list);

	*pres = res;
	return EV_OK;
}

// TODO: implement
ev_code_t ev_sig_on(ev_signo_t sig) {
	(void)sig;
	return EV_OK;
}
ev_code_t ev_sig_off(ev_signo_t sig) {
	(void)sig;
	return EV_OK;
}
ev_code_t ev_sig_wait(ev_signo_t *sig) {
	(void)sig;
	// Since signals aren't implemneted, the correct behavior here is to block indefinitely
	while (true) {
		Sleep(1000);
	}
}
ev_code_t evq_sig_wait(ev_req_t req, ev_signo_t *sig) {
	(void)sig;

	// Completely ignoring this request makes sure its never delivered
	evi_req_begin(req, NULL);
	return EV_OK;
}

ev_code_t ev_getpath(ev_path_type_t type, char **pres) {
	switch (type) {
		case EV_PATH_HOME: {
			char *res = _evi_win_getpath(CSIDL_PROFILE, NULL);
			if (!res) return evi_win_conv_errno(GetLastError());

			*pres = res;
			return EV_OK;
		}
		case EV_PATH_RUNTIME:
		case EV_PATH_CACHE: {
			char *res = _evi_win_getpath(CSIDL_LOCAL_APPDATA, L"\\Temp");
			if (!res) return evi_win_conv_errno(GetLastError());

			*pres = res;
			return EV_OK;
		}
		case EV_PATH_CONFIG: {
			char *res = _evi_win_getpath(CSIDL_APPDATA, NULL);
			if (!res) return evi_win_conv_errno(GetLastError());

			*pres = res;
			return EV_OK;
		}
		case EV_PATH_DATA: {
			char *res = _evi_win_getpath(CSIDL_LOCAL_APPDATA, NULL);
			if (!res) return evi_win_conv_errno(GetLastError());

			*pres = res;
			return EV_OK;
		}
		case EV_PATH_CWD: {
			wchar_t *wpath = malloc(sizeof *wpath * (MAX_PATH + 1));
			if (!wpath) return EV_ENOMEM;
			if (!GetCurrentDirectoryW(sizeof *wpath * (MAX_PATH + 1), wpath)) return evi_win_conv_errno(GetLastError());

			*pres = evi_win_conv_utf16(wpath);
			free(wpath);
			if (!*pres) return evi_win_conv_errno(GetLastError());
			return EV_OK;
		}
	}

	return EV_EINVAL;
}

ev_code_t ev_getenv(const char *name, char **pres) {
	wchar_t *wname = evi_win_conv_utf8(name, 0);
	if (!wname) return evi_win_conv_errno(GetLastError());

	int n = GetEnvironmentVariableW(wname, NULL, 0);
	if (!n) {
		free(wname);
		if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
			*pres = NULL;
			return EV_OK;
		}

		return evi_win_conv_errno(GetLastError());
	}

	wchar_t *buff = malloc(sizeof *buff * n);
	if (!buff) {
		free(wname);
		return EV_ENOMEM;
	}

	if (!GetEnvironmentVariableW(wname, buff, n)) {
		free(wname);
		free(buff);
		return evi_win_conv_errno(GetLastError());
	}

	free(wname);
	*pres = evi_win_conv_utf16(buff);
	free(buff);
	if (!*pres) return evi_win_conv_errno(GetLastError());
	return EV_OK;
}
ev_code_t ev_setenv(const char *name, const char *val) {
	wchar_t *wname = evi_win_conv_utf8(name, 0);
	if (!wname) return evi_win_conv_errno(GetLastError());

	wchar_t *wval = evi_win_conv_utf8(val, 0);
	if (!wval) {
		free(wname);
		return evi_win_conv_errno(GetLastError());
	}

	bool res = SetEnvironmentVariableW(wname, wval);
	free(wname);
	free(wval);
	if (!res) return evi_win_conv_errno(GetLastError());
	return EV_OK;
}

ev_code_t ev_enviter_new(ev_enviter_t *pres) {
	ev_enviter_t res = malloc(sizeof *res);
	if (!res) return EV_ENOMEM;

	res->data = res->curr = GetEnvironmentStringsW();
	if (!res->data) {
		free(res);
		return evi_win_conv_errno(GetLastError());
	}

	*pres = res;
	return EV_OK;
}
ev_code_t ev_enviter_next(ev_enviter_t iter, const char **pres) {
	free(iter->lastalloc);

	size_t n = wcslen(iter->curr);
	if (n == 0) {
		*pres = NULL;
		return EV_OK;
	}

	char *pair = evi_win_conv_utf16(iter->curr);
	if (!pair) return evi_win_conv_errno(GetLastError());
	iter->lastalloc = pair;
	iter->curr += n + 1;

	*pres = pair;
	return EV_OK;
}
void ev_enviter_close(ev_enviter_t iter) {
	FreeEnvironmentStringsW(iter->data);
	free(iter);
}

ev_time_t ev_time(ev_clock_t clock) {
	switch (clock) {
		case EV_CLOCK_REALTIME: {
			FILETIME time;
			GetSystemTimePreciseAsFileTime(&time);
			return evi_win_conv_filetime(time);
		}
		case EV_CLOCK_MONOTIME: {
			LARGE_INTEGER counter, freq;
			QueryPerformanceCounter(&counter);
			QueryPerformanceFrequency(&freq);

			return (ev_time_t) {
				.sec = counter.QuadPart / freq.QuadPart,
				.nsec = (uint64_t)(counter.QuadPart % freq.QuadPart) * 1000000000LL / freq.QuadPart,
			};
		}
		case EV_CLOCK_CPUTIME: {
			FILETIME kernel, user;
			GetThreadTimes(GetCurrentThread(), NULL, NULL, &kernel, &user);
			return ev_timeadd(evi_win_conv_filetime(kernel), evi_win_conv_filetime(user));
		}
		default: return (ev_time_t) { 0, 0 };
	}
}
void ev_timesleep(ev_time_t until) {
	Sleep(ev_timems(ev_timesub(until, ev_time(EV_CLOCK_MONOTIME))));
}

#define evq_sig_wait(...) evq_sig_wait(__VA_ARGS__)
