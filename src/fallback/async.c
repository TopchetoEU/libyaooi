#pragma once

/*
One of the more complicated files, so i think an explanation is in order:

Since not all operations are implemented or implementable using the async mechanism of choice,
Some operations must go thru the thread pool. In order to not repeat a lot of code, this macro spaghetti is used
In short, it generates a structure to pack the arguments of each IO op, a worker function for the thread pool and
an implementation of the ev.h interface function.
*/

#include <yaioi/conf.h>
#include <yaioi/signo.h>

#include <stddef.h>
#include <stdlib.h>

#include "../impl.c"

#define YOI_COMMA ,
#define YOI_SEMICOLON ;

#define YOI_FALLBACK_STRUCT_ARG_X(type, name) type name
#define YOI_FALLBACK_PARAM_ARG_X(type, name) type name
#define YOI_FALLBACK_CONSTRUCT_ARG_X(type, name) pargs->name = name
#define YOI_FALLBACK_DECONSTRUCT_ARG_X(type, name) args.name

#define YOI_MKFALLBACK(name, args_x) \
	typedef struct { \
		args_x(YOI_FALLBACK_STRUCT_ARG_X, YOI_SEMICOLON); \
	} yoi_##name##_args_t; \
\
	static int yoi_##name##_worker(void *pargs) { \
		yoi_##name##_args_t args = *(yoi_##name##_args_t*)pargs; \
		free(pargs); \
		return yo_##name(args_x(YOI_FALLBACK_DECONSTRUCT_ARG_X, YOI_COMMA)); \
	} \
	yo_code_t yoa_##name(yo_req_t req, args_x(YOI_FALLBACK_PARAM_ARG_X, YOI_COMMA)) {\
		yoi_##name##_args_t *pargs = malloc(sizeof *pargs);\
		if (!pargs) return YO_ENOMEM; \
\
		args_x(YOI_FALLBACK_CONSTRUCT_ARG_X, YOI_SEMICOLON); \
		return yo_req_exec(req, yoi_##name##_worker, pargs); \
	}

#define YOI_READ_PARAMS(ARG, SEP) ARG(yo_fd_t, handle) SEP ARG(char*, buff) SEP ARG(size_t*, pn)
#define YOI_WRITE_PARAMS(ARG, SEP) ARG(yo_fd_t, handle) SEP ARG(char*, buff) SEP ARG(size_t*, pn)
#define YOI_SYNC_PARAMS(ARG, SEP) ARG(yo_fd_t, fd)
#define YOI_STAT_PARAMS(ARG, SEP) ARG(yo_fd_t, fd) SEP ARG(yo_stat_t*, buff)

// #define YOI_FILE_OPEN_PARAMS(ARG, SEP) ARG(yo_fd_t*, pres) SEP ARG(const char*, path) SEP ARG(yo_open_flags_t, flags) SEP ARG(int, mode)
#define YOI_FILE_READ_PARAMS(ARG, SEP) ARG(yo_fd_t, handle) SEP ARG(char*, buff) SEP ARG(size_t*, pn) SEP ARG(size_t, offset)
#define YOI_FILE_WRITE_PARAMS(ARG, SEP) ARG(yo_fd_t, handle) SEP ARG(char*, buff) SEP ARG(size_t*, pn) SEP ARG(size_t, offset)
#define YOI_FILE_CHMOD_PARAMS(ARG, SEP) ARG(yo_fd_t, hnd) SEP ARG(int, mode)
#define YOI_FILE_CHOWN_PARAMS(ARG, SEP) ARG(yo_fd_t, hnd) SEP ARG(int, uid) SEP ARG(int, gid)
#define YOI_FILE_SYMLINK_PARAMS(ARG, SEP) ARG(const char*, path) SEP ARG(const char*, target)
#define YOI_FILE_HARDLINK_PARAMS(ARG, SEP) ARG(const char*, path) SEP ARG(const char*, target)
#define YOI_FILE_READLINK_PARAMS(ARG, SEP) ARG(const char*, path) SEP ARG(char**, pres)
#define YOI_FILE_REMOVE_PARAMS(ARG, SEP) ARG(const char*, path)

#define YOI_DIR_NEW_PARAMS(ARG, SEP) ARG(const char*, path) SEP ARG(int, mode)
#define YOI_DIR_OPEN_PARAMS(ARG, SEP) ARG(yo_dir_t*, pres) SEP ARG(const char*, path)
#define YOI_DIR_NEXT_PARAMS(ARG, SEP) ARG(yo_dir_t, dir) SEP ARG(char**, pname)

#define YOI_SOCKET_CONNECT_PARAMS(ARG, SEP) ARG(yo_fd_t*, pres) SEP ARG(yo_proto_t, proto) SEP ARG(yo_addr_t, addr) SEP ARG(uint16_t, port)
#define YOI_SOCKET_ACCEPT_PARAMS(ARG, SEP) ARG(yo_fd_t, server) SEP ARG(yo_fd_t*, pres) SEP ARG(yo_addr_t*, paddr) SEP ARG(uint16_t*, pport)

#define YOI_PROC_SPAWN_PARAMS(ARG, SEP) \
	ARG(yo_proc_t*, pres) SEP ARG(yo_spawn_flags_t, flags) SEP \
	ARG(const char**, argv) SEP ARG(const char**, env) SEP ARG(const char*, cwd) SEP \
	ARG(yo_fd_t*, pin) SEP ARG(yo_fd_t*, pout) SEP ARG(yo_fd_t*, perr)
#define YOI_PROC_WAIT_PARAMS(ARG, SEP) ARG(yo_proc_t, proc) SEP ARG(int*, psig) SEP ARG(int*, pcode)

#define YOI_DNS_GETADDRINFO_PARAMS(ARG, SEP) ARG(yo_addrinfo_t*, pres) SEP ARG(const char*, name) SEP ARG(yo_addrinfo_flags_t, flags)

#define YOI_SIG_WAIT_PARAMS(ARG, SEP) ARG(yo_signo_t*, pres)

#ifndef yoa_read
	YOI_MKFALLBACK(read, YOI_READ_PARAMS)
#endif
#ifndef yoa_write
	YOI_MKFALLBACK(write, YOI_WRITE_PARAMS)
#endif

#ifndef yoa_sync
	YOI_MKFALLBACK(sync, YOI_SYNC_PARAMS)
#endif
#ifndef yoa_stat
	YOI_MKFALLBACK(stat, YOI_STAT_PARAMS)
#endif
// #ifndef yoa_file_open
// 	YOI_MKFALLBACK(file_open, YOI_FILE_OPEN_PARAMS)
// #endif
#ifndef yoa_file_read
	YOI_MKFALLBACK(file_read, YOI_FILE_READ_PARAMS)
#endif
#ifndef yoa_file_write
	YOI_MKFALLBACK(file_write, YOI_FILE_WRITE_PARAMS)
#endif
#ifndef yoa_dir_new
	YOI_MKFALLBACK(dir_new, YOI_DIR_NEW_PARAMS)
#endif
#ifndef yoa_dir_open
	YOI_MKFALLBACK(dir_open, YOI_DIR_OPEN_PARAMS)
#endif
#ifndef yoa_dir_next
	YOI_MKFALLBACK(dir_next, YOI_DIR_NEXT_PARAMS)
#endif
#ifndef yoa_socket_connect
	YOI_MKFALLBACK(socket_connect, YOI_SOCKET_CONNECT_PARAMS)
#endif
#ifndef yoa_socket_accept
	YOI_MKFALLBACK(socket_accept, YOI_SOCKET_ACCEPT_PARAMS)
#endif
#ifndef yoa_proc_spawn
	YOI_MKFALLBACK(proc_spawn, YOI_PROC_SPAWN_PARAMS)
#endif
#ifndef yoa_proc_wait
	YOI_MKFALLBACK(proc_wait, YOI_PROC_WAIT_PARAMS)
#endif
#ifndef yoa_dns_getaddrinfo
	YOI_MKFALLBACK(dns_getaddrinfo, YOI_DNS_GETADDRINFO_PARAMS)
#endif
#ifndef yoa_sig_wait
	YOI_MKFALLBACK(sig_wait, YOI_SIG_WAIT_PARAMS)
#endif

#ifndef yoa_file_symlink
	YOI_MKFALLBACK(file_symlink, YOI_FILE_SYMLINK_PARAMS)
#endif
#ifndef yoa_file_hardlink
	YOI_MKFALLBACK(file_hardlink, YOI_FILE_HARDLINK_PARAMS)
#endif
#ifndef yoa_file_readlink
	YOI_MKFALLBACK(file_readlink, YOI_FILE_READLINK_PARAMS)
#endif
#ifndef yoa_file_chmod
	YOI_MKFALLBACK(file_chmod, YOI_FILE_CHMOD_PARAMS)
#endif
#ifndef yoa_file_chown
	YOI_MKFALLBACK(file_chown, YOI_FILE_CHOWN_PARAMS)
#endif
#ifndef yoa_file_remove
	YOI_MKFALLBACK(file_remove, YOI_FILE_REMOVE_PARAMS)
#endif
