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

#include <yaioi/conf.h>
#include <yaioi/errno.h>
#include <yaioi/io.h>
#include <yaioi/queue.h>

#include "./impl.h" // IWYU pragma: export

#include "../fallback/queue.c" // IWYU pragma: export
#include "../queue.c"
#include "ev/time.h"

// FIXME: never before run code, shat it out in an evening.
// Consider windows as unsupported, until I can be bothered to cross-compile luajit

#define COMBINE64(a, b) (((uint64_t)(b) << 32) | (a))

static bool _yoi_win_initialized = false;

static void _yoi_win_init() {
	if (_yoi_win_initialized) return;
	_yoi_win_initialized = true;

	WSADATA data;
	WSAStartup(MAKEWORD(2, 2), &data);
	// ev->in = yoi_win_mkhnd(GetStdHandle(STD_INPUT_HANDLE));
	// ev->out = yoi_win_mkhnd(GetStdHandle(STD_OUTPUT_HANDLE));
	// ev->err = yoi_win_mkhnd(GetStdHandle(STD_ERROR_HANDLE));

	// return YO_OK;
}

static char *_yoi_win_getpath(int id, const wchar_t *suffix) {
	wchar_t *buff = suffix ? malloc(sizeof *buff * (MAX_PATH + wcslen(suffix) + 1)) : malloc(sizeof *buff * (MAX_PATH + 1));
	if (!buff) return NULL;
	if (SHGetFolderPathW(NULL, id, NULL, 0, buff) != S_OK) return NULL;

	if (suffix) wcscpy(buff, suffix);

	char *res = yoi_win_conv_utf16(buff);
	free(buff);
	return res;
}
static SOCKET _yoi_win_sock_new(yo_proto_t proto, yo_addr_type_t type) {
	return socket(
		type == YO_ADDR_IPV4 ? AF_INET : AF_INET6,
		proto == YO_PROTO_UDP ? SOCK_DGRAM : SOCK_STREAM,
		proto == YO_PROTO_UDP ? IPPROTO_UDP : IPPROTO_TCP
	);
}

static int _yoi_win_child_std_new(bool in, HANDLE *pparent, HANDLE *pchild, yo_fd_t *pnew) {
	SECURITY_ATTRIBUTES attribs = { .nLength = sizeof attribs, .bInheritHandle = true };

	yo_fd_t res = malloc(sizeof *res);
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
static wchar_t *_yoi_win_argv_to_cmdline_raw(const char **argv) {
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

		wchar_t *warg = yoi_win_conv_utf8(argv[i], 0);
		if (warg == NULL) {
			free(buff);
			return NULL;
		}

		wcscat(buff, warg);
	}

	return buff;
}
static wchar_t *_yoi_win_argv_to_cmdline(const char **argv, yo_spawn_flags_t flags) {
	// Horray for windows-specific weirdness (I hate this OS with a burning passion)
	if (flags == YO_SPAWN_NOESCAPE) {
		return _yoi_win_argv_to_cmdline_raw(argv);
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
		wchar_t *warg = yoi_win_conv_utf8(argv[i], 0);
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
static wchar_t *_yoi_win_envp_to_envblock(const char **envp) {
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

static void yoi_win_mkhnd(yo_fd_t res, HANDLE hnd) {
	res->owned = true;
	res->kind = YOI_WIN_HND;
	res->hnd = hnd;
}
static void yoi_win_mksock(yo_fd_t res, SOCKET sock) {
	res->owned = true;
	res->kind = YOI_WIN_SOCK;
	res->sock = sock;
}

static wchar_t *yoi_win_fix_path(wchar_t *path) {
	for (wchar_t *it = wcschr(path, '/'); it; it = wcschr(path, '/')) {
		*it = '\\';
	}

	return path;
}
// Everybody uses utf8, but NOOOOO, windows just HAD to use utf16
static wchar_t *yoi_win_conv_utf8(const char *str, size_t extra_n) {
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
static char *yoi_win_conv_utf16(const wchar_t *wstr) {
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

static yo_time_t yoi_win_conv_filetime(FILETIME filetime) {
	static const uint64_t EPOCH_DIFFERENCE = 11644473600;

	uint64_t total_ticks = ((uint64_t)filetime.dwHighDateTime << 32) | (uint64_t)filetime.dwLowDateTime;

	return (yo_time_t) {
		.sec = (time_t)(total_ticks / 100000000) - EPOCH_DIFFERENCE,
		.nsec = (long)(total_ticks % 100000000) * 100,
	};
}
static yo_code_t yoi_win_conv_errno(int winerr) {
	switch (winerr) {
		case ERROR_ACCESS_DENIED: return YO_EPERM;
		case ERROR_ACTIVE_CONNECTIONS: return YO_EAGAIN;
		case ERROR_ADDRESS_ALREADY_ASSOCIATED:  return YO_EADDRINUSE;
		case ERROR_ALREADY_EXISTS: return YO_EEXIST;
		case ERROR_BAD_DEVICE: return YO_ENODEV;
		case ERROR_BAD_EXE_FORMAT: return YO_ENOEXEC;
		case ERROR_BAD_NET_NAME: return YO_ENOENT;
		case ERROR_BAD_NET_RESP: return YO_ENOSYS;
		case ERROR_BAD_NETPATH: return YO_ENOENT;
		case ERROR_BAD_PATHNAME: return YO_ENOENT;
		case ERROR_BAD_PIPE: return YO_EPIPE;
		case ERROR_BAD_UNIT: return YO_ENODEV;
		case ERROR_BAD_USERNAME: return YO_EINVAL;
		case ERROR_BEGINNING_OF_MEDIA: return YO_EIO;
		// case ERROR_BROKEN_PIPE: return YO_EOF;
		case ERROR_BROKEN_PIPE: return YO_EPIPE;
		case ERROR_BUFFER_OVERFLOW: return YO_EFAULT;
		case ERROR_BUS_RESET: return YO_EIO;
		case ERROR_BUSY: return YO_EBUSY;
		case ERROR_CALL_NOT_IMPLEMENTED: return YO_ENOSYS;
		case ERROR_CANCELLED: return YO_EINTR;
		case ERROR_CANNOT_MAKE: return YO_EPERM;
		case ERROR_CANT_RESOLVE_FILENAME: return YO_ELOOP;
		case ERROR_CHILD_NOT_COMPLETE: return YO_EBUSY;
		case ERROR_COMMITMENT_LIMIT: return YO_EAGAIN;
		case ERROR_CONNECTION_ABORTED: return YO_ECONNABORTED;
		case ERROR_CONNECTION_REFUSED: return YO_ECONNREFUSED;
		case ERROR_CRC: return YO_EIO;
		case ERROR_DYO_NOT_EXIST: return YO_ENOENT;
		case ERROR_DEVICE_DOOR_OPEN: return YO_EIO;
		case ERROR_DEVICE_IN_USE: return YO_EAGAIN;
		case ERROR_DEVICE_REQUIRES_CLEANING: return YO_EIO;
		case ERROR_DIR_NOT_EMPTY: return YO_ENOTEMPTY;
		case ERROR_DIRECTORY: return YO_ENOTDIR;
		case ERROR_DISK_CORRUPT: return YO_EIO;
		case ERROR_DISK_FULL: return YO_ENOSPC;
		case ERROR_DS_GENERIC_ERROR: return YO_EIO;
		case ERROR_DUP_NAME: return YO_ENOTUNIQ;
		case ERROR_EA_LIST_INCONSISTENT: return YO_EINVAL;
		case ERROR_EA_TABLE_FULL: return YO_ENOSPC;
		case ERROR_EAS_DIDNT_FIT: return YO_ENOSPC;
		case ERROR_EAS_NOT_SUPPORTED: return YO_ENOTSUP;
		case ERROR_END_OF_MEDIA: return YO_ENOSPC;
		case ERROR_EOM_OVERFLOW: return YO_EIO;
		case ERROR_EXE_MACHINE_TYPE_MISMATCH: return YO_ENOEXEC;
		case ERROR_EXE_MARKED_INVALID: return YO_ENOEXEC;
		case ERROR_FILE_CORRUPT: return YO_EEXIST;
		case ERROR_FILE_EXISTS: return YO_EEXIST;
		case ERROR_FILE_INVALID: return YO_ENXIO;
		case ERROR_FILE_NOT_FOUND: return YO_ENOENT;
		case ERROR_FILEMARK_DETECTED: return YO_EIO;
		case ERROR_FILENAME_EXCED_RANGE: return YO_ENAMETOOLONG;
		case ERROR_GEN_FAILURE: return YO_EIO;
		case ERROR_HANDLE_DISK_FULL: return YO_ENOSPC;
		case ERROR_HANDLE_EOF: return YO_ENODATA;
		case ERROR_HOST_UNREACHABLE: return YO_EHOSTUNREACH;
		case ERROR_INSUFFICIENT_BUFFER: return YO_EINVAL;
		case ERROR_INVALID_ADDRESS: return YO_EINVAL;
		case ERROR_INVALID_AT_INTERRUPT_TIME: return YO_EINTR;
		case ERROR_INVALID_BLOCK_LENGTH: return YO_EIO;
		case ERROR_INVALID_DATA: return YO_EINVAL;
		case ERROR_INVALID_DRIVE: return YO_ENODEV;
		case ERROR_INVALID_EA_NAME: return YO_EINVAL;
		case ERROR_INVALID_EXE_SIGNATURE: return YO_ENOEXEC;
		case ERROR_INVALID_FLAGS: return YO_EBADF;
		case ERROR_INVALID_FUNCTION: return YO_EISDIR;
		// case ERROR_INVALID_FUNCTION: return YO_EINVAL;
		case ERROR_INVALID_HANDLE: return YO_EBADF;
		case ERROR_INVALID_NAME: return YO_ENOENT;
		case ERROR_INVALID_PARAMETER: return YO_EINVAL;
		case ERROR_INVALID_REPARSE_DATA: return YO_ENOENT;
		case ERROR_INVALID_SIGNAL_NUMBER: return YO_EINVAL;
		case ERROR_IO_DEVICE: return YO_EIO;
		case ERROR_IO_INCOMPLETE: return YO_EAGAIN;
		case ERROR_IO_PENDING: return YO_EAGAIN;
		case ERROR_IOPL_NOT_ENABLED: return YO_ENOEXEC;
		case ERROR_LOCK_VIOLATION: return YO_EBUSY;
		case ERROR_MAX_THRDS_REACHED: return YO_EAGAIN;
		case ERROR_META_EXPANSION_TOO_LONG: return YO_EINVAL;
		case ERROR_MOD_NOT_FOUND: return YO_ENOENT;
		case ERROR_MORE_DATA: return YO_EMSGSIZE;
		case ERROR_NEGATIVE_SEEK: return YO_EINVAL;
		case ERROR_NETNAME_DELETED: return YO_ECONNRESET;
		case ERROR_NETWORK_UNREACHABLE: return YO_ENETUNREACH;
		case ERROR_NO_DATA_DETECTED: return YO_EIO;
		case ERROR_NO_DATA: return YO_EPIPE;
		case ERROR_NO_MEDIA_IN_DRIVE: return YO_ENOMEDIUM;
		case ERROR_NO_MORE_FILES: return YO_ENOENT;
		case ERROR_NO_MORE_ITEMS: return YO_ENOENT;
		case ERROR_NO_MORE_SEARCH_HANDLES: return YO_ENFILE;
		case ERROR_NO_PROC_SLOTS: return YO_EAGAIN;
		case ERROR_NO_SIGNAL_SENT: return YO_EIO;
		case ERROR_NO_SYSTEM_RESOURCES: return YO_EFBIG;
		case ERROR_NO_TOKEN: return YO_EINVAL;
		case ERROR_NO_UNICODE_TRANSLATION: return YO_ECHARSET;
		case ERROR_NOACCESS: return YO_EACCES;
		case ERROR_NONE_MAPPED: return YO_EINVAL;
		case ERROR_NONPAGED_SYSTEM_RESOURCES: return YO_EAGAIN;
		case ERROR_NOT_CONNECTED: return YO_ENOTCONN;
		case ERROR_NOT_ENOUGH_MEMORY: return YO_ENOMEM;
		case ERROR_NOT_ENOUGH_QUOTA: return YO_EIO;
		case ERROR_NOT_OWNER: return YO_EPERM;
		case ERROR_NOT_READY: return YO_ENOMEDIUM;
		case ERROR_NOT_SAME_DEVICE: return YO_EXDEV;
		case ERROR_NOT_SUPPORTED: return YO_ENOTSUP;
		case ERROR_OPEN_FAILED: return YO_EIO;
		case ERROR_OPEN_FILES: return YO_EAGAIN;
		case ERROR_OPERATION_ABORTED: return YO_ECANCELED;
		case ERROR_OUTOFMEMORY: return YO_ENOMEM;
		case ERROR_PAGED_SYSTEM_RESOURCES: return YO_EAGAIN;
		case ERROR_PAGEFILE_QUOTA: return YO_EAGAIN;
		case ERROR_PATH_NOT_FOUND: return YO_ENOENT;
		case ERROR_PIPE_BUSY: return YO_EBUSY;
		case ERROR_PIPE_CONNECTED: return YO_EBUSY;
		case ERROR_PIPE_LISTENING: return YO_ECOMM;
		case ERROR_PIPE_NOT_CONNECTED: return YO_EPIPE;
		case ERROR_POSSIBLE_DEADLOCK: return YO_EDEADLK;
		case ERROR_PRIVILEGE_NOT_HELD: return YO_EPERM;
		case ERROR_PROC_NOT_FOUND: return YO_ESRCH;
		case ERROR_PROCESS_ABORTED: return YO_EFAULT;
		case ERROR_REM_NOT_LIST: return YO_ENOENT;
		case ERROR_SECTOR_NOT_FOUND: return YO_EINVAL;
		case ERROR_SEEK: return YO_ESPIPE;
		case ERROR_SEM_TIMEOUT: return YO_ETIMEDOUT;
		case ERROR_SERVICE_REQUEST_TIMEOUT: return YO_ETIMEDOUT;
		case ERROR_SETMARK_DETECTED: return YO_EIO;
		case ERROR_SHARING_BUFFER_EXCEEDED: return YO_ENOLCK;
		case ERROR_SHARING_VIOLATION: return YO_EBUSY;
		case ERROR_SIGNAL_PENDING: return YO_EBUSY;
		case ERROR_SIGNAL_REFUSED: return YO_EIO;
		case ERROR_SXS_CANT_GEN_ACTCTX: return YO_ELIBBAD;
		case ERROR_SYMLINK_NOT_SUPPORTED: return YO_EINVAL;
		case ERROR_THREAD_1_INACTIVE: return YO_EINVAL;
		case ERROR_TIMEOUT: return YO_EBUSY;
		case ERROR_TOO_MANY_LINKS: return YO_EMLINK;
		case ERROR_TOO_MANY_OPEN_FILES: return YO_EMFILE;
		case ERROR_UNEXP_NET_ERR: return YO_EIO;
		case ERROR_WAIT_NO_CHILDREN: return YO_ECHILD;
		case ERROR_WORKING_SET_QUOTA: return YO_EAGAIN;
		case ERROR_WRITE_PROTECT: return YO_EROFS;
		case ERROR_RESOURCE_DATA_NOT_FOUND: return YO_ENOEXEC;
		case ERROR_RESOURCE_TYPE_NOT_FOUND: return YO_ENOEXEC;
		case ERROR_RESOURCE_NAME_NOT_FOUND: return YO_ENOEXEC;
		case ERROR_RESOURCE_LANG_NOT_FOUND: return YO_ENOEXEC;

		case WSAEACCES: return YO_EACCES;
		case WSAEADDRINUSE: return YO_EADDRINUSE;
		case WSAEADDRNOTAVAIL: return YO_EADDRNOTAVAIL;
		case WSAEAFNOSUPPORT: return YO_EAFNOSUPPORT;
		case WSAEALREADY: return YO_EALREADY;
		case WSAECONNABORTED: return YO_ECONNABORTED;
		case WSAECONNREFUSED: return YO_ECONNREFUSED;
		case WSAECONNRESET: return YO_ECONNRESET;
		case WSAEFAULT: return YO_EFAULT;
		case WSAEHOSTUNREACH: return YO_EHOSTUNREACH;
		case WSAEINTR: return YO_EINTR;
		case WSAEINVAL: return YO_EINVAL;
		case WSAEISCONN: return YO_EISCONN;
		case WSAEMFILE: return YO_EMFILE;
		case WSAEMSGSIZE: return YO_EMSGSIZE;
		case WSAENETUNREACH: return YO_ENETUNREACH;
		case WSAENOBUFS: return YO_ENOBUFS;
		case WSAENOTCONN: return YO_ENOTCONN;
		case WSAENOTSOCK: return YO_ENOTSOCK;
		case WSAEPFNOSUPPORT: return YO_EPFNOSUPPORT;
		case WSAEPROTONOSUPPORT: return YO_EPROTONOSUPPORT;
		case WSAESHUTDOWN: return YO_EPIPE;
		case WSAESOCKTNOSUPPORT: return YO_ESOCKTNOSUPPORT;
		case WSAETIMEDOUT: return YO_ETIMEDOUT;
		case WSAEWOULDBLOCK: return YO_EAGAIN;
		case WSAHOST_NOT_FOUND: return YO_ENOENT;
		case WSANO_DATA: return YO_ENOENT;

		default: return YO_EUNKNOWN;
	}
}

static int yoi_win_conv_addr(yo_addr_t addr, uint16_t port, struct sockaddr_storage *pres) {
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
static void yoi_win_conv_sockaddr(struct sockaddr_storage *sockaddr, yo_addr_t *pres, uint16_t *pport) {
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

		// for (size_t i = 0; i < 8; i++) {
		// 	pres->v6[i] = ntohs(pres->v6[i]);
		// }
	}
}

yo_code_t yo_fd_new(yo_fd_t *pres, uint64_t fd, bool owned) {
	yo_fd_t res = malloc(sizeof *res);
	if (!res) return YO_ENOMEM;

	yoi_win_mkhnd(res, (HANDLE)fd);
	res->owned = owned;

	*pres = res;
	return YO_OK;
}
void yo_fd_close(yo_fd_t fd) {
	if (fd->owned) {
		switch (fd->kind) {
			case YOI_WIN_HND: CloseHandle(fd->hnd); break;
			case YOI_WIN_SOCK: closesocket(fd->sock); break;
		}
	}

	free(fd);
}

yo_code_t yo_read(yo_fd_t fd, char *buff, size_t *pn) {
	switch (fd->kind) {
		case YOI_WIN_HND: {
			DWORD out_n;

			if (!ReadFile(fd->hnd, (void*)buff, *pn, &out_n, NULL)) {
				if (GetLastError() == ERROR_HANDLE_EOF || GetLastError() == ERROR_BROKEN_PIPE) {
					*pn = 0;
					return YO_OK;
				}

				return yoi_win_conv_errno(GetLastError());
			}
			*pn = out_n;
			return YO_OK;
		}
		case YOI_WIN_SOCK: {
			int res = recv(fd->sock, (void*)buff, *pn, 0);
			if (res < 0) return yoi_win_conv_errno(WSAGetLastError());

			*pn = res;
			return YO_OK;
		}
		default: return YO_EBADF;
	}
}
yo_code_t yo_write(yo_fd_t fd, char *buff, size_t *pn) {
	switch (fd->kind) {
		case YOI_WIN_HND: {
			DWORD out_n;

			if (!WriteFile(fd->hnd, (void*)buff, *pn, &out_n, NULL)) {
				if (GetLastError() == ERROR_HANDLE_EOF || GetLastError() == ERROR_BROKEN_PIPE) {
					*pn = 0;
					return YO_OK;
				}

				return yoi_win_conv_errno(GetLastError());
			}
			*pn = out_n;
			return YO_OK;
		}
		case YOI_WIN_SOCK: {
			int res = send(fd->sock, (void*)buff, *pn, 0);
			if (res < 0) return yoi_win_conv_errno(WSAGetLastError());

			*pn = res;
			return YO_OK;
		}
		default: return YO_EBADF;
	}
}
yo_code_t yo_sync(yo_fd_t fd) {
	if (fd->kind != YOI_WIN_HND) return YO_EBADF;
	if (!FlushFileBuffers(fd->hnd)) return yoi_win_conv_errno(GetLastError());
	return YO_OK;
}
yo_code_t yo_stat(yo_fd_t fd, yo_stat_t *buff) {
	if (fd->kind != YOI_WIN_HND) return YO_EBADF;

	BY_HANDLE_FILE_INFORMATION info;
	if (!GetFileInformationByHandle(fd->hnd, &info)) return yoi_win_conv_errno(GetLastError());

	// Fake it till we make it .-.

	if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
		buff->mode = 0555;
	}
	else {
		buff->mode = 0777;
	}

	if (info.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) {
		buff->type = YO_STAT_REG;
	}
	else if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		buff->type = YO_STAT_DIR;
	}
	else if (info.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) {
		buff->type = YO_STAT_BLK;
	}
	else if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
		buff->type = YO_STAT_LINK;
		buff->mode = 0777;
	}

	buff->uid = 1000;
	buff->gid = 1000;

	buff->atime = yoi_win_conv_filetime(info.ftLastAccessTime);
	buff->mtime = yoi_win_conv_filetime(info.ftLastWriteTime);
	// The semantics of this are dubious, do we equate the creation of a file to the last change of its metadata
	// TODO: look into this further
	buff->ctime = yoi_win_conv_filetime(info.ftCreationTime);

	buff->size = COMBINE64(info.nFileSizeLow, info.nFileSizeHigh);
	// TODO: does windows expose a preferred blk size somehow? For now, we pick a reasonable arbitrary blksize
	buff->blksize = 4096;

	buff->inode = COMBINE64(info.nFileIndexLow, info.nFileIndexHigh);
	buff->links = info.nNumberOfLinks;

	return YO_OK;
}

yo_code_t yo_tty_in(yo_fd_t *pres) {
	return yo_fd_new(pres, (uint64_t)(size_t)GetStdHandle(STD_INPUT_HANDLE), false);
}
yo_code_t yo_tty_out(yo_fd_t *pres) {
	return yo_fd_new(pres, (uint64_t)(size_t)GetStdHandle(STD_OUTPUT_HANDLE), false);
}
yo_code_t yo_tty_err(yo_fd_t *pres) {
	return yo_fd_new(pres, (uint64_t)(size_t)GetStdHandle(STD_ERROR_HANDLE), false);
}
yo_code_t yo_tty_raw(yo_fd_t tty, yo_tty_raw_t *pres) {
	(void)tty, (void)pres;
	// Not yet...
	return YO_ENOTSUP;
}
yo_code_t yo_tty_rawend(yo_tty_raw_t rawmode) {
	(void)rawmode;
	return YO_ENOTSUP;
}

yo_code_t yo_file_remove(const char *path) {
	wchar_t *wpath = yoi_win_conv_utf8(path, 0);
	if (!wpath) return yoi_win_conv_errno(GetLastError());

	if (!DeleteFileW(wpath)) {
		if (GetLastError() != ERROR_ACCESS_DENIED) {
			free(wpath);
			return yoi_win_conv_errno(GetLastError());
		}
	}
	if (!RemoveDirectoryW(wpath)) {
		free(wpath);
		return yoi_win_conv_errno(GetLastError());
	}

	free(wpath);
	return YO_OK;
}
yo_code_t yo_file_symlink(const char *path, const char *target) {
	wchar_t *wpath = yoi_win_conv_utf8(path, 0);
	if (!wpath) return yoi_win_conv_errno(GetLastError());

	wchar_t *wtarget = yoi_win_conv_utf8(target, 0);
	if (!wpath) {
		free(wpath);
		return yoi_win_conv_errno(GetLastError());
	}

	// TODO: handle directories
	bool res = CreateSymbolicLinkW(wtarget, wpath, 0);
	free(wpath);
	free(wtarget);

	if (!res) return yoi_win_conv_errno(GetLastError());
	return YO_OK;
}
yo_code_t yo_file_hardlink(const char *path, const char *target) {
	wchar_t *wpath = yoi_win_conv_utf8(path, 0);
	if (!wpath) return yoi_win_conv_errno(GetLastError());

	wchar_t *wtarget = yoi_win_conv_utf8(target, 0);
	if (!wpath) {
		free(wpath);
		return yoi_win_conv_errno(GetLastError());
	}

	// TODO: handle directories
	bool res = CreateHardLinkW(wtarget, wpath, 0);
	free(wpath);
	free(wtarget);

	if (!res) return yoi_win_conv_errno(GetLastError());
	return YO_OK;
}
yo_code_t yo_file_readlink(const char *path, char **pres) {
	(void)path, (void)pres;
	// Tough luck
	return YO_ENOTSUP;
}

yo_code_t yo_file_open(yo_fd_t *pres, const char *path, yo_open_flags_t flags, int mode) {
	(void)mode;

	yo_fd_t res = malloc(sizeof *res);
	if (!res) return YO_ENOMEM;

	DWORD access = 0;
	DWORD access_others = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	DWORD create_mode = 0;
	DWORD res_flags = 0;
	SECURITY_ATTRIBUTES sec_attribs = { 0 };
	sec_attribs.nLength = sizeof sec_attribs;

	if (!(flags & YO_OPEN_STAT)) {
		if (flags & YO_OPEN_APPEND) {
			flags |= YO_OPEN_WRITE;
		}

		if (flags & YO_OPEN_READ) {
			access |= FILE_GENERIC_READ;
			access_others &= ~(FILE_SHARE_DELETE);
		}
		if (flags & YO_OPEN_WRITE) {
			access |= FILE_GENERIC_WRITE;
			access_others &= ~(FILE_SHARE_DELETE | FILE_SHARE_WRITE);

			if (!(flags & YO_OPEN_APPEND)) {
				access &= ~FILE_APPEND_DATA;
			}
		}
	}

	switch (flags & (YO_OPEN_CREATE | YO_OPEN_TRUNC)) {
		case 0: create_mode = OPEN_EXISTING; break;
		case YO_OPEN_CREATE: create_mode = OPEN_ALWAYS; break;
		case YO_OPEN_TRUNC: create_mode = TRUNCATE_EXISTING; break;
		case YO_OPEN_CREATE | YO_OPEN_TRUNC: create_mode = CREATE_ALWAYS; break;
	}

	if (flags & YO_OPEN_DIRECT) {
		res_flags |= FILE_FLAG_WRITE_THROUGH;
	}

	if (flags & YO_OPEN_SHARED) {
		sec_attribs.bInheritHandle = true;
	}

	wchar_t *wpath = yoi_win_conv_utf8(path, 0);
	if (!wpath) {
		free(res);
		return yoi_win_conv_errno(GetLastError());
	}

	HANDLE hnd = CreateFileW(wpath, access, access_others, NULL, create_mode, FILE_ATTRIBUTE_NORMAL | res_flags, NULL);
	free(wpath);
	if (hnd == INVALID_HANDLE_VALUE) {
		free(res);
		return yoi_win_conv_errno(GetLastError());
	}

	yoi_win_mkhnd(res, hnd);

	*pres = res;
	return YO_OK;
}
yo_code_t yo_file_read(yo_fd_t fd, char *buff, size_t *n, size_t offset) {
	if (fd->kind != YOI_WIN_HND) return YO_EBADF;

	DWORD out_n;
	OVERLAPPED overlapped = { .Pointer = (void*)offset };

	if (!ReadFile(fd->hnd, (void*)buff, *n, &out_n, &overlapped)) {
		if (GetLastError() == ERROR_HANDLE_EOF || GetLastError() == ERROR_BROKEN_PIPE) {
			*n = 0;
			return YO_OK;
		}

		return yoi_win_conv_errno(GetLastError());
	}
	*n = out_n;
	return YO_OK;
}
yo_code_t yo_file_write(yo_fd_t fd, char *buff, size_t *n, size_t offset) {
	if (fd->kind != YOI_WIN_HND) return YO_EBADF;

	DWORD out_n;
	OVERLAPPED overlapped = { .Pointer = (void*)offset };

	if (!WriteFile(fd->hnd, buff, *n, &out_n, &overlapped)) {
		if (GetLastError() == ERROR_HANDLE_EOF) {
			*n = 0;
			return YO_OK;
		}

		return yoi_win_conv_errno(GetLastError());
	}
	*n = out_n;
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
	(void)mode;

	wchar_t *wpath = yoi_win_conv_utf8(path, 0);
	if (!wpath) return yoi_win_conv_errno(GetLastError());

	bool res = CreateDirectoryW(wpath, NULL);
	free(wpath);
	if (!res) return yoi_win_conv_errno(GetLastError());

	return YO_OK;
}
yo_code_t yo_dir_open(yo_dir_t *pres, const char *path) {
	wchar_t *wpattern = yoi_win_conv_utf8(path, 2);
	if (!wpattern) return yoi_win_conv_errno(GetLastError());
	wcscat(wpattern, L"\\*");
	yoi_win_fix_path(wpattern);

	WIN32_FIND_DATAW data;
	memset(&data, 0, sizeof data);
	HANDLE hnd = FindFirstFileW(wpattern, &data);
	free(wpattern);
	if (hnd == INVALID_HANDLE_VALUE) return yoi_win_conv_errno(GetLastError());

	yo_dir_t res = malloc(sizeof *res);
	if (!res) return YO_ENOMEM;

	res->data = data;
	res->hnd = hnd;
	res->done = false;

	*pres = res;
	return YO_OK;
}
yo_code_t yo_dir_next(yo_dir_t dir, char **pname) {
	while (true) {
		if (dir->done) {
			*pname = NULL;
			return YO_OK;
		}

		bool is_synth = !wcscmp(dir->data.cFileName, L".") || !wcscmp(dir->data.cFileName, L"..");
		if (!is_synth) *pname = yoi_win_conv_utf16(dir->data.cFileName);

		if (!FindNextFileW(dir->hnd, &dir->data)) {
			if (GetLastError() == ERROR_NO_MORE_FILES) {
				dir->done = true;
			}
			else {
				return yoi_win_conv_errno(GetLastError());
			}
		}

		if (!is_synth) return YO_OK;
	}
}
void yo_dir_close(yo_dir_t dir) {
	FindClose(dir->hnd);
	free(dir);
}

yo_code_t yo_socket_bind(yo_fd_t *pres, yo_proto_t proto, yo_addr_t addr, uint16_t port, size_t max_n) {
	_yoi_win_init();

	yo_fd_t res = malloc(sizeof *res);
	if (!res) return YO_ENOMEM;

	SOCKET sock = _yoi_win_sock_new(proto, addr.type);
	if (sock == INVALID_SOCKET) {
		free(res);
		return yoi_win_conv_errno(WSAGetLastError());
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*)&(int) { 1 }, sizeof(int)) < 0) {
		free(res);
		closesocket(sock);
		return yoi_win_conv_errno(WSAGetLastError());
	}

	struct sockaddr_storage arg_addr;
	int len = yoi_win_conv_addr(addr, port, &arg_addr);

	if (bind(sock, (void*)&arg_addr, len) < 0) {
		free(res);
		closesocket(sock);
		return yoi_win_conv_errno(WSAGetLastError());
	}
	if (listen(sock, max_n) < 0) {
		free(res);
		closesocket(sock);
		return yoi_win_conv_errno(WSAGetLastError());
	}

	yoi_win_mksock(res, sock);

	*pres = res;
	return YO_OK;
}
yo_code_t yo_socket_accept(yo_fd_t server, yo_fd_t *pres, yo_addr_t *paddr, uint16_t *pport) {
	_yoi_win_init();

	yo_fd_t res = malloc(sizeof *res);
	if (!res) return YO_ENOMEM;

	struct sockaddr_storage addr = {};
	socklen_t addr_len = sizeof addr;

	SOCKET client = accept((SOCKET)(size_t)server, (void*)&addr, &addr_len);
	if (!client) {
		free(res);
		return yoi_win_conv_errno(WSAGetLastError());
	}

	yoi_win_conv_sockaddr(&addr, paddr, pport);
	yoi_win_mksock(res, client);

	*pres = (void*)(size_t)client;
	return YO_OK;
}
yo_code_t yo_socket_connect(yo_fd_t *pres, yo_proto_t proto, yo_addr_t addr, uint16_t port) {
	_yoi_win_init();

	yo_fd_t res = malloc(sizeof *res);
	if (!res) return YO_ENOMEM;

	SOCKET sock = _yoi_win_sock_new(proto, addr.type);
	if (sock == INVALID_SOCKET) {
		free(res);
		return yoi_win_conv_errno(WSAGetLastError());
	}

	struct sockaddr_storage arg_addr;
	int len = yoi_win_conv_addr(addr, port, &arg_addr);

	if (connect(sock, (void*)&arg_addr, len) < 0) {
		closesocket(sock);
		free(res);
		return yoi_win_conv_errno(WSAGetLastError());
	}

	yoi_win_mksock(res, sock);

	*pres = res;
	return YO_OK;
}

yo_code_t yo_dns_getaddrinfo(yo_addrinfo_t *pres, const char *name, yo_addrinfo_flags_t flags) {
	ADDRINFOW hints = { 0 };

	if (flags & YO_AI_IPV4_MAPPED) hints.ai_flags |= YO_AI_IPV4_MAPPED;

	if (flags & YO_AI_IPV6) hints.ai_family = AF_INET6;
	else if (flags & YO_AI_IPV4) hints.ai_family = AF_INET;
	else hints.ai_family = AF_UNSPEC;

	if (flags & YO_AI_BIND) hints.ai_flags |= AI_PASSIVE;
	if (flags & YO_AI_NODNS) hints.ai_flags |= AI_NUMERICHOST;

	ADDRINFOW *list = NULL;

	wchar_t *wname = yoi_win_conv_utf8(name, 0);
	if (!wname) return yoi_win_conv_errno(GetLastError());

	int code;

	// We still want to resolve a valid loopback IP, even if getaddrinfo
	if (name == NULL) code = GetAddrInfoW(wname, L"80", &hints, &list);
	else code = GetAddrInfoW(wname, L"", &hints, &list);

	free(wname);

	switch (code) {
		case 0: break;
		case EAI_NODATA: break;
		case EAI_NONAME: break;
		case EAI_SOCKTYPE: return YO_EINVAL;
		case EAI_BADFLAGS: return YO_EINVAL;
		case EAI_FAMILY: return YO_ENOTSUP;
		case EAI_MEMORY: return YO_ENOMEM;
		case EAI_AGAIN: return YO_EAGAIN;
		case EAI_FAIL: return YO_EIO;
	}

	size_t n = 0;
	for (ADDRINFOW *it = list; it; it = it->ai_next) n++;

	yo_addrinfo_t res = malloc(sizeof *res + sizeof *res->addr * n);
	if (!res) return YO_ENOMEM;

	size_t i = 0;
	for (ADDRINFOW *it = list; it; it = it->ai_next) {
		uint16_t port;
		yo_addr_t addr;
		yoi_win_conv_sockaddr((void*)it->ai_addr, &addr, &port);

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
	return YO_OK;
}

yo_code_t yo_proc_spawn(
	yo_proc_t *pres, yo_spawn_flags_t flags,
	const char **argv, const char **envp, const char *cwd,
	yo_fd_t *pin,
	yo_fd_t *pout,
	yo_fd_t *perr
) {
	HANDLE in_parent = NULL, in_child = NULL;
	HANDLE out_parent = NULL, out_child = NULL;
	HANDLE err_parent = NULL, err_child = NULL;

	yo_fd_t in_res = NULL, out_res = NULL, err_res = NULL;

	if (pin) {
		if (_yoi_win_child_std_new(true, &in_parent, &in_child, &in_res) < 0) goto err;
	}
	if (pout) {
		if (_yoi_win_child_std_new(false, &out_parent, &out_child, &out_res) < 0) goto err_in_pipe;
	}
	if (perr) {
		if (_yoi_win_child_std_new(false, &err_parent, &err_child, &err_res) < 0) goto err_out_pipe;
	}

	wchar_t *cmdline = _yoi_win_argv_to_cmdline(argv, flags);
	if (!cmdline) goto err_err_pipe;

	wchar_t *envblock = _yoi_win_envp_to_envblock(envp);
	if (!envblock) goto err_cmdline;

	wchar_t *procname = yoi_win_conv_utf8(argv[0], 0);
	if (!procname) goto err_envblock;

	wchar_t *wcwd = yoi_win_conv_utf8(cwd, 0);
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
		yoi_win_mkhnd(in_res, in_parent);
		*pin = in_res;
	}
	if (pout) {
		yoi_win_mkhnd(out_res, out_parent);
		*pout = out_res;
	}
	if (perr) {
		yoi_win_mkhnd(err_res, err_parent);
		*perr = err_res;
	}

	*pres = proc_info.hProcess;
	CloseHandle(proc_info.hThread);

	return YO_OK;
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
	return yoi_win_conv_errno(GetLastError());
}
yo_code_t yo_proc_wait(yo_proc_t proc, int *psig, int *pcode) {
	switch (WaitForSingleObject(proc->hnd, INFINITE)) {
		case WAIT_ABANDONED:
			return YO_EDEADLK;
		case WAIT_OBJECT_0:
			break;
		case WAIT_TIMEOUT:
			return YO_ETIMEDOUT;
		case WAIT_FAILED:
			return yoi_win_conv_errno(GetLastError());
	}

	DWORD code;
	if (!GetExitCodeProcess(proc->hnd, &code)) return yoi_win_conv_errno(GetLastError());

	CloseHandle(proc->hnd);

	*psig = -1;
	*pcode = code;

	return YO_OK;
}
yo_code_t yo_proc_disown(yo_proc_t proc) {
	CloseHandle(proc->hnd);
	free(proc);
	return YO_OK;
}

// TODO: implement
yo_code_t yo_sig_on(yo_signo_t sig) {
	(void)sig;
	return YO_OK;
}
yo_code_t yo_sig_off(yo_signo_t sig) {
	(void)sig;
	return YO_OK;
}
yo_code_t yo_sig_wait(yo_signo_t *sig) {
	(void)sig;
	// Since signals aren't implemneted, the correct behavior here is to block indefinitely
	while (true) {
		Sleep(1000);
	}
}
yo_code_t yoa_sig_wait(yo_req_t req, yo_signo_t *sig) {
	(void)sig;

	// Completely ignoring this request makes sure its never delivered
	yoi_req_begin(req, yoi_req_cancel_noop_cb);
	return YO_OK;
}

yo_code_t yo_getpath(yo_path_type_t type, char **pres) {
	switch (type) {
		case YO_PATH_HOME: {
			char *res = _yoi_win_getpath(CSIDL_PROFILE, NULL);
			if (!res) return yoi_win_conv_errno(GetLastError());

			*pres = res;
			return YO_OK;
		}
		case YO_PATH_RUNTIME:
		case YO_PATH_CACHE: {
			char *res = _yoi_win_getpath(CSIDL_LOCAL_APPDATA, L"\\Temp");
			if (!res) return yoi_win_conv_errno(GetLastError());

			*pres = res;
			return YO_OK;
		}
		case YO_PATH_CONFIG: {
			char *res = _yoi_win_getpath(CSIDL_APPDATA, NULL);
			if (!res) return yoi_win_conv_errno(GetLastError());

			*pres = res;
			return YO_OK;
		}
		case YO_PATH_DATA: {
			char *res = _yoi_win_getpath(CSIDL_LOCAL_APPDATA, NULL);
			if (!res) return yoi_win_conv_errno(GetLastError());

			*pres = res;
			return YO_OK;
		}
		case YO_PATH_CWD: {
			wchar_t *wpath = malloc(sizeof *wpath * (MAX_PATH + 1));
			if (!wpath) return YO_ENOMEM;
			if (!GetCurrentDirectoryW(sizeof *wpath * (MAX_PATH + 1), wpath)) return yoi_win_conv_errno(GetLastError());

			*pres = yoi_win_conv_utf16(wpath);
			free(wpath);
			if (!*pres) return yoi_win_conv_errno(GetLastError());
			return YO_OK;
		}
	}

	return YO_EINVAL;
}

yo_code_t yo_env_get(const char *name, char **pres) {
	wchar_t *wname = yoi_win_conv_utf8(name, 0);
	if (!wname) return yoi_win_conv_errno(GetLastError());

	int n = GetEnvironmentVariableW(wname, NULL, 0);
	if (!n) {
		free(wname);
		if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
			*pres = NULL;
			return YO_OK;
		}

		return yoi_win_conv_errno(GetLastError());
	}

	wchar_t *buff = malloc(sizeof *buff * n);
	if (!buff) {
		free(wname);
		return YO_ENOMEM;
	}

	if (!GetEnvironmentVariableW(wname, buff, n)) {
		free(wname);
		free(buff);
		return yoi_win_conv_errno(GetLastError());
	}

	free(wname);
	*pres = yoi_win_conv_utf16(buff);
	free(buff);
	if (!*pres) return yoi_win_conv_errno(GetLastError());
	return YO_OK;
}
yo_code_t yo_env_set(const char *name, const char *val) {
	wchar_t *wname = yoi_win_conv_utf8(name, 0);
	if (!wname) return yoi_win_conv_errno(GetLastError());

	wchar_t *wval = yoi_win_conv_utf8(val, 0);
	if (!wval) {
		free(wname);
		return yoi_win_conv_errno(GetLastError());
	}

	bool res = SetEnvironmentVariableW(wname, wval);
	free(wname);
	free(wval);
	if (!res) return yoi_win_conv_errno(GetLastError());
	return YO_OK;
}

yo_enviter_t yo_enviter_new() {
	yo_enviter_t res = malloc(sizeof *res);
	if (!res) return NULL;

	res->data = res->curr = GetEnvironmentStringsW();
	if (!res->data) {
		free(res);
		return yoi_win_conv_errno(GetLastError());
	}

	return res;
}
yo_code_t yo_enviter_next(yo_enviter_t iter, const char **pres) {
	free(iter->lastalloc);

	size_t n = wcslen(iter->curr);
	if (n == 0) {
		*pres = NULL;
		return YO_OK;
	}

	char *pair = yoi_win_conv_utf16(iter->curr);
	if (!pair) return yoi_win_conv_errno(GetLastError());
	iter->lastalloc = pair;
	iter->curr += n + 1;

	*pres = pair;
	return YO_OK;
}
void yo_enviter_close(yo_enviter_t iter) {
	FreeEnvironmentStringsW(iter->data);
	free(iter);
}

yo_time_t yo_time(yo_clock_t clock) {
	switch (clock) {
		case YO_CLOCK_REALTIME: {
			FILETIME time;
			GetSystemTimePreciseAsFileTime(&time);
			return yoi_win_conv_filetime(time);
		}
		case YO_CLOCK_MONOTIME: {
			LARGE_INTEGER counter, freq;
			QueryPerformanceCounter(&counter);
			QueryPerformanceFrequency(&freq);

			return (yo_time_t) {
				.sec = counter.QuadPart / freq.QuadPart,
				.nsec = (uint64_t)(counter.QuadPart % freq.QuadPart) * 1000000000LL / freq.QuadPart,
			};
		}
		case YO_CLOCK_CPUTIME: {
			FILETIME kernel, user;
			GetThreadTimes(GetCurrentThread(), NULL, NULL, &kernel, &user);
			return yo_timeadd(yoi_win_conv_filetime(kernel), yoi_win_conv_filetime(user));
		}
		default: return (yo_time_t) { 0, 0 };
	}
}
void yo_timesleep(yo_time_t until) {
	Sleep(yo_timems(yo_timesub(until, yo_time(YO_CLOCK_MONOTIME))));
}

#define yoa_sig_wait(...) yoa_sig_wait(__VA_ARGS__)
