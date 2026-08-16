#pragma once

/*
One of the more complicated files, so i think an explanation is in order:

Since not all operations are implemented or implementable using the async mechanism of choice,
Some operations must go thru the thread pool. In order to not repeat a lot of code, this macro spaghetti is used
In short, it generates a structure to pack the arguments of each IO op, a worker function for the thread pool and
an implementation of the ev.h interface function.
*/

#include <ev/conf.h>
#include <ev/signo.h>
#include <ev/filelist.h>

#include <stddef.h>
#include <stdlib.h>

#include "../impl.c"

#define EVI_COMMA ,
#define EVI_SEMICOLON ;

#define EVI_FALLBACK_STRUCT_ARG_X(type, name) type name
#define EVI_FALLBACK_PARAM_ARG_X(type, name) type name
#define EVI_FALLBACK_CONSTRUCT_ARG_X(type, name) pargs->name = name
#define EVI_FALLBACK_DECONSTRUCT_ARG_X(type, name) args.name

#define EVI_MKFALLBACK(name, args_x) \
	typedef struct { \
		args_x(EVI_FALLBACK_STRUCT_ARG_X, EVI_SEMICOLON); \
	} evi_##name##_args_t; \
\
	static int evi_##name##_worker(void *pargs) { \
		evi_##name##_args_t args = *(evi_##name##_args_t*)pargs; \
		free(pargs); \
		return ev_##name(args_x(EVI_FALLBACK_DECONSTRUCT_ARG_X, EVI_COMMA)); \
	} \
	ev_code_t evq_##name(ev_req_t req, args_x(EVI_FALLBACK_PARAM_ARG_X, EVI_COMMA)) {\
		evi_##name##_args_t *pargs = malloc(sizeof *pargs);\
		if (!pargs) return EV_ENOMEM; \
\
		args_x(EVI_FALLBACK_CONSTRUCT_ARG_X, EVI_SEMICOLON); \
		return ev_req_exec(req, evi_##name##_worker, pargs); \
	}

#define EVI_READ_PARAMS(ARG, SEP) ARG(ev_fd_t, handle) SEP ARG(char*, buff) SEP ARG(size_t*, pn)
#define EVI_WRITE_PARAMS(ARG, SEP) ARG(ev_fd_t, handle) SEP ARG(char*, buff) SEP ARG(size_t*, pn)
#define EVI_SYNC_PARAMS(ARG, SEP) ARG(ev_fd_t, fd)
#define EVI_STAT_PARAMS(ARG, SEP) ARG(ev_fd_t, fd) SEP ARG(ev_stat_t*, buff)

// #define EVI_FILE_OPEN_PARAMS(ARG, SEP) ARG(ev_filelist_t, fl) SEP ARG(ev_fd_t*, pres) SEP ARG(const char*, path) SEP ARG(ev_open_flags_t, flags) SEP ARG(int, mode)
#define EVI_FILE_READ_PARAMS(ARG, SEP) ARG(ev_fd_t, handle) SEP ARG(char*, buff) SEP ARG(size_t*, pn) SEP ARG(size_t, offset)
#define EVI_FILE_WRITE_PARAMS(ARG, SEP) ARG(ev_fd_t, handle) SEP ARG(char*, buff) SEP ARG(size_t*, pn) SEP ARG(size_t, offset)
#define EVI_FILE_CHMOD_PARAMS(ARG, SEP) ARG(ev_fd_t, hnd) SEP ARG(int, mode)
#define EVI_FILE_CHOWN_PARAMS(ARG, SEP) ARG(ev_fd_t, hnd) SEP ARG(int, uid) SEP ARG(int, gid)
#define EVI_FILE_SYMLINK_PARAMS(ARG, SEP) ARG(const char*, path) SEP ARG(const char*, target)
#define EVI_FILE_HARDLINK_PARAMS(ARG, SEP) ARG(const char*, path) SEP ARG(const char*, target)
#define EVI_FILE_READLINK_PARAMS(ARG, SEP) ARG(const char*, path) SEP ARG(char**, pres)
#define EVI_FILE_REMOVE_PARAMS(ARG, SEP) ARG(const char*, path)

#define EVI_DIR_NEW_PARAMS(ARG, SEP) ARG(const char*, path) SEP ARG(int, mode)
#define EVI_DIR_OPEN_PARAMS(ARG, SEP) ARG(ev_filelist_t, fl) SEP ARG(ev_dir_t*, pres) SEP ARG(const char*, path)
#define EVI_DIR_NEXT_PARAMS(ARG, SEP) ARG(ev_dir_t, dir) SEP ARG(char**, pname)

#define EVI_SOCKET_CONNECT_PARAMS(ARG, SEP) ARG(ev_filelist_t, fl) SEP ARG(ev_fd_t*, pres) SEP ARG(ev_proto_t, proto) SEP ARG(ev_addr_t, addr) SEP ARG(uint16_t, port)
#define EVI_SOCKET_ACCEPT_PARAMS(ARG, SEP) ARG(ev_filelist_t, fl) SEP ARG(ev_fd_t, server) SEP ARG(ev_fd_t*, pres) SEP ARG(ev_addr_t*, paddr) SEP ARG(uint16_t*, pport)

#define EVI_PROC_SPAWN_PARAMS(ARG, SEP) \
	ARG(ev_filelist_t, fl) SEP \
	ARG(ev_proc_t*, pres) SEP \
	ARG(const char**, argv) SEP \
	ARG(const char**, env) SEP \
	ARG(const char*, cwd) SEP \
	ARG(ev_fd_t*, pin) SEP \
	ARG(ev_fd_t*, pout) SEP \
	ARG(ev_fd_t*, perr)
#define EVI_PROC_WAIT_PARAMS(ARG, SEP) \
	ARG(ev_proc_t, proc) SEP \
	ARG(int*, psig) SEP \
	ARG(int*, pcode)

#define EVI_DNS_GETADDRINFO_PARAMS(ARG, SEP) ARG(ev_addrinfo_t*, pres) SEP ARG(const char*, name) SEP ARG(ev_addrinfo_flags_t, flags)

#define EVI_SIG_WAIT_PARAMS(ARG, SEP) ARG(ev_signo_t*, pres)

#ifndef evq_read
	EVI_MKFALLBACK(read, EVI_READ_PARAMS)
#endif
#ifndef evq_write
	EVI_MKFALLBACK(write, EVI_WRITE_PARAMS)
#endif

#ifndef evq_sync
	EVI_MKFALLBACK(sync, EVI_SYNC_PARAMS)
#endif
#ifndef evq_stat
	EVI_MKFALLBACK(stat, EVI_STAT_PARAMS)
#endif
// #ifndef evq_file_open
// 	EVI_MKFALLBACK(file_open, EVI_FILE_OPEN_PARAMS)
// #endif
#ifndef evq_file_read
	EVI_MKFALLBACK(file_read, EVI_FILE_READ_PARAMS)
#endif
#ifndef evq_file_write
	EVI_MKFALLBACK(file_write, EVI_FILE_WRITE_PARAMS)
#endif
#ifndef evq_dir_new
	EVI_MKFALLBACK(dir_new, EVI_DIR_NEW_PARAMS)
#endif
#ifndef evq_dir_open
	EVI_MKFALLBACK(dir_open, EVI_DIR_OPEN_PARAMS)
#endif
#ifndef evq_dir_next
	EVI_MKFALLBACK(dir_next, EVI_DIR_NEXT_PARAMS)
#endif
#ifndef evq_socket_connect
	EVI_MKFALLBACK(socket_connect, EVI_SOCKET_CONNECT_PARAMS)
#endif
#ifndef evq_socket_accept
	EVI_MKFALLBACK(socket_accept, EVI_SOCKET_ACCEPT_PARAMS)
#endif
#ifndef evq_proc_spawn
	EVI_MKFALLBACK(proc_spawn, EVI_PROC_SPAWN_PARAMS)
#endif
#ifndef evq_proc_wait
	EVI_MKFALLBACK(proc_wait, EVI_PROC_WAIT_PARAMS)
#endif
#ifndef evq_dns_getaddrinfo
	EVI_MKFALLBACK(dns_getaddrinfo, EVI_DNS_GETADDRINFO_PARAMS)
#endif
#ifndef evq_sig_wait
	EVI_MKFALLBACK(sig_wait, EVI_SIG_WAIT_PARAMS)
#endif

#ifndef evq_file_symlink
	EVI_MKFALLBACK(file_symlink, EVI_FILE_SYMLINK_PARAMS)
#endif
#ifndef evq_file_hardlink
	EVI_MKFALLBACK(file_hardlink, EVI_FILE_HARDLINK_PARAMS)
#endif
#ifndef evq_file_readlink
	EVI_MKFALLBACK(file_readlink, EVI_FILE_READLINK_PARAMS)
#endif
#ifndef evq_file_chmod
	EVI_MKFALLBACK(file_chmod, EVI_FILE_CHMOD_PARAMS)
#endif
#ifndef evq_file_chown
	EVI_MKFALLBACK(file_chown, EVI_FILE_CHOWN_PARAMS)
#endif
#ifndef evq_file_remove
	EVI_MKFALLBACK(file_remove, EVI_FILE_REMOVE_PARAMS)
#endif
