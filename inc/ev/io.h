#ifndef EV_IO_H
#define EV_IO_H

#include <ev/filelist.h>
#include <ev/queue.h>
#include <ev/addr.h>
#include <ev/errno.h>
#include <ev/signo.h>

// These are the I/O wrapper functions - they will return 0 on success and a negative errno code on error
// All the other arguments are self-explanatory. All of these functions return their results in a pointer, provided by the callee

// Some functions here are async - they take a second parameter of `ev_req_t req`. If such a function returns `EV_EWOULDBLCOK`, you must wait
// for the passed request to be returned by ev_poll. If you pass a NULL ev_req_t, they will block (and never return EV_EWOULDBLCOK).

typedef enum {
	// Opens the file in read mode
	EV_OPEN_READ = 1,
	// Opens the file in write mode
	EV_OPEN_WRITE = 2,
	// Opens the file in append mode (implies WRITE)
	EV_OPEN_APPEND = 4,

	// Creates the file if it doesn't exist
	EV_OPEN_CREATE = 8,
	// Empties the contents of the file if it exists
	EV_OPEN_TRUNC = 16,
	// Opens the file in direct mode
	EV_OPEN_DIRECT = 32,
	// Keeps the file open after an exec() call
	// By default, all files, not marked with this, are closed
	EV_OPEN_SHARED = 64,

	// Doesn't follow symlinks. Useful for statting
	EV_OPEN_NOFOLLOW = 128,
	// Opens the file in statting mode. Mutually-exclusive with READ, WRITE and APPEND and takes precedence over them
	EV_OPEN_STAT= 128,
} ev_open_flags_t;
typedef enum {
	EV_PATH_HOME,
	EV_PATH_CONFIG,
	EV_PATH_DATA,
	EV_PATH_CACHE,
	EV_PATH_RUNTIME,
	EV_PATH_CWD,
} ev_path_type_t;
typedef enum {
	EV_PROTO_TCP,
	EV_PROTO_UDP,
} ev_proto_t;
typedef enum {
	EV_TTY_NORMAL,
	EV_TTY_RAW,
} ev_tty_mode_t;

typedef struct {
	enum {
		EV_STAT_REG,
		EV_STAT_DIR,
		EV_STAT_LINK,
		EV_STAT_SOCK,
		EV_STAT_FIFO,
		EV_STAT_CHAR,
		EV_STAT_BLK,
	} type;
	uint32_t mode;
	uint32_t gid;
	uint32_t uid;

	ev_time_t atime, mtime, ctime;

	uint64_t size;
	uint32_t blksize;

	uint64_t inode;
	uint32_t links;
} ev_stat_t;

// A handle roughly equates to a fd (or a windows HANDLE/socket). Such may be an opened file, socket, tty or a pipe.
typedef struct ev_fd *ev_fd_t;

// Creates a handle from an OS-specific FD
ev_code_t ev_fd_new(ev_filelist_t fl, ev_fd_t *pres, uint64_t fd);
// Cancels all requests, associated to the handle and releases all resources, used by the `req`
void ev_fd_close(ev_fd_t fd);

// Equivalent to posix's read
ev_code_t ev_read(ev_fd_t fd, char *buff, size_t *pn);
// Equivalent to posix's write
ev_code_t ev_write(ev_fd_t fd, char *buff, size_t *pn);
// Equivalent to posix's sync
ev_code_t ev_sync(ev_fd_t fd);
// Equivalent to posix's stat
ev_code_t ev_stat(ev_fd_t fd, ev_stat_t *buff);

typedef struct ev_tty_raw *ev_tty_raw_t;

// Initializes `tty` to a refrence to `stdin`
ev_code_t ev_tty_in(ev_filelist_t fl, ev_fd_t *pres);
// Initializes `tty` to a refrence to `stdout`
ev_code_t ev_tty_out(ev_filelist_t fl, ev_fd_t *pres);
// Initializes `tty` to a refrence to `stderr`
ev_code_t ev_tty_err(ev_filelist_t fl, ev_fd_t *pres);
// Begins a raw mode of the tty. ev_tty_rawend must be called on ev_tty_raw_t, stored in pres, to end the raw mode
// Make sure to do that, as we are not calling that for you upon exit!
ev_code_t ev_tty_raw(ev_fd_t tty, ev_tty_raw_t *pres);
// Restores the given raw mode to the previous mode of the underlying TTY. This may be another raw mode
ev_code_t ev_tty_rawend(ev_tty_raw_t rawmode);

// Deletes the given file or directory. Fails if directory is not empty
ev_code_t ev_file_remove(const char *EV_NONULL path);
// Creates a symbolic link to path at target
ev_code_t ev_file_symlink(const char *EV_NONULL src, const char *EV_NONULL dst);
// Creates a hard link to the file
ev_code_t ev_file_hardlink(const char *EV_NONULL src, const char *EV_NONULL dst);
// Reads the given symlink into a malloc'd string
ev_code_t ev_file_readlink(const char *EV_NONULL path, char **pres);

// Although on linux, files are blocking, the file functions are async, because they may block for a long time
// (for example, if the file lives on an NFS or FUSE filesystem)

// Equivalent to posix's open
ev_code_t ev_file_open(ev_filelist_t fl, ev_fd_t *pres, const char *path, ev_open_flags_t flags, int mode);
// A file-specific read function
ev_code_t ev_file_read(ev_fd_t fd, char *buff, size_t *pn, size_t offset);
// A file-specific write function
ev_code_t ev_file_write(ev_fd_t fd, char *buff, size_t *pn, size_t offset);
// Changes the permissions of the given file
ev_code_t ev_file_chmod(ev_fd_t fd, int mode);
// Changes the owner of the given file
ev_code_t ev_file_chown(ev_fd_t fd, int uid, int gid);

typedef struct ev_dir *ev_dir_t;
// Equivalent to posix's mkdir
ev_code_t ev_dir_new(const char *EV_NONULL path, int mode);
// Equivalent to posix's opendir
ev_code_t ev_dir_open(ev_filelist_t fl, ev_dir_t *pres, const char *EV_NONULL path);
// Equivalent to posix's readdir
ev_code_t ev_dir_next(ev_dir_t dir, char **EV_NONULL pname);
// Equivalent to posix's closedir
void ev_dir_close(ev_dir_t dir);

// Equivalent to connect()
ev_code_t ev_socket_connect(ev_filelist_t fl, ev_fd_t *pres, ev_proto_t proto, ev_addr_t addr, uint16_t port);
// Equivalent to bind()
ev_code_t ev_socket_bind(ev_filelist_t fl, ev_fd_t *pres, ev_proto_t proto, ev_addr_t addr, uint16_t port, size_t max_n);
// Equivalent to accept()
ev_code_t ev_socket_accept(ev_filelist_t fl, ev_fd_t server, ev_fd_t *pres, ev_addr_t *EV_NONULL paddr, uint16_t *EV_NONULL pport);

typedef struct {
	size_t n;
	ev_addr_t addr[];
} *ev_addrinfo_t;
typedef enum {
	// Resolves only ipv4 (if neither this nor EV_AI_IPV6 are specified, resolves both)
	EV_AI_IPV4 = 1,
	// Resolves only ipv6 (this is mutually-exclusive with IPV4, and this will override IPV4)
	EV_AI_IPV6 = 2,
	// If no IPV6 address was found, but an IPV4 address was, resolves as an ipv6 mapping of the ipv4 address
	EV_AI_IPV4_MAPPED = 4,
	// Resolves to a bindable address - mostly applicable when name is NULL (equivalent to AI_PASSIVE)
	EV_AI_BIND = 8,
	// Resolves only IP addresses - does not make DNS requests (equivalent to AI_NUMERICHOST)
	EV_AI_NODNS = 16,
} ev_addrinfo_flags_t;

// Equivalent to posix's getaddrinfo (with a few simplifications)
ev_code_t ev_dns_getaddrinfo(ev_addrinfo_t *EV_NONULL pres, const char *EV_NONULL name, ev_addrinfo_flags_t flags);

typedef struct ev_proc *ev_proc_t;

// Equivalent to posix's fork then exec
ev_code_t ev_proc_spawn(
	ev_filelist_t fl, ev_proc_t *pres,
	const char **EV_NONULL argv, const char **env, const char *cwd,
	ev_fd_t *pin, ev_fd_t *pout, ev_fd_t *perr
);
// Equivalent to posix's waitpid
// Will free all resources, associated with proc
// psig is set to the signal that terminated the child, or -1 if not terminated by a signal
// pcode is set to the exit code of the app, or -1 if child did not exit with a code
ev_code_t ev_proc_wait(ev_proc_t proc, int *EV_NONULL psig, int *EV_NONULL pcode);
// Immeditely releases all resources, associated with tracking the process, effectively daemonizing it.
// On unix-like systems, this will initialize a process-wide reaper (if not initialized yet) and put the child in the reaper's list
// Upon our process's exit, as per unix rules, the child will daemonize
ev_code_t ev_proc_disown(ev_proc_t proc);

// Signal handling utilities. NOTE: these won't correlate to signals 1:1, as signals have a stupid amount of historic baggage
// Activating one logical ev signal might activate multiple OS signals, or none at all. Furthermore, the set of signals you can
// receive has been reduced to ones you will want to receive.

// On windows, signals don't exist, so they are "faked" with other facilities.
// This means that some ev signals will never be produced on windows.

// Activates the given signal for receiving. After this call, wait_sig will receive this signal, when generated, as well
// Internally, both this and ev_sig_off use a refcount, so the two must be called in pairs (calling off is optional,
// but it must be called no more times than on has been called per signal)
ev_code_t ev_sig_on(ev_signo_t sig);
// Deactivates the given signal and restores its default semantics. After this call, wait_sig will no longer receiv eit
ev_code_t ev_sig_off(ev_signo_t sig);
// Blocks until the given signal is received.
// NOTE: activating a signal and then not calling sig_wait is equivalent to ignoring it
ev_code_t ev_sig_wait(ev_signo_t *EV_NONULL pres);

// Gets a malloc'd string, representing the requested path
ev_code_t ev_getpath(ev_path_type_t type, char **EV_NONULL pres);

// Gets an env variable from the current process
ev_code_t ev_env_get(const char *EV_NONULL name, char **EV_NONULL pres);
// Sets an env variable in the current process (if val is NULL, unsets it)
ev_code_t ev_env_set(const char *EV_NONULL name, const char *val);

typedef struct ev_enviter *ev_enviter_t;
// Initializes an iterator of the env variables
ev_code_t ev_enviter_new(ev_enviter_t *pres);
// Gets the next env variable from the iterator
ev_code_t ev_enviter_next(ev_enviter_t iter, const char **EV_NONULL pres);
void ev_enviter_close(ev_enviter_t iter);

#endif
