#ifndef YO_IO_H
#define YO_IO_H

#include <yaooi/queue.h>
#include <yaooi/addr.h>
#include <yaooi/errno.h>
#include <yaooi/signo.h>

// These are the I/O wrapper functions - they will return 0 on success and a negative errno code on error
// All the other arguments are self-explanatory. All of these functions return their results in a pointer, provided by the callee

// Some functions here are async - they take a second parameter of `yo_req_t req`. If such a function returns `YO_EWOULDBLCOK`, you must wait
// for the passed request to be returned by yo_poll. If you pass a NULL yo_req_t, they will block (and never return YO_EWOULDBLCOK).

typedef enum {
	// Opens the file in read mode
	YO_OPEN_READ = 1,
	// Opens the file in write mode
	YO_OPEN_WRITE = 2,
	// Opens the file in append mode (implies WRITE)
	YO_OPEN_APPEND = 4,

	// Creates the file if it doesn't exist
	YO_OPEN_CREATE = 8,
	// Empties the contents of the file if it exists
	YO_OPEN_TRUNC = 16,
	// Opens the file in direct mode
	YO_OPEN_DIRECT = 32,
	// Keeps the file open after an exec() call
	// By default, all files, not marked with this, are closed
	YO_OPEN_SHARED = 64,

	// Doesn't follow symlinks. Useful for statting
	YO_OPEN_NOFOLLOW = 128,
	// Opens the file in statting mode. Mutually-exclusive with READ, WRITE and APPEND and takes precedence over them
	YO_OPEN_STAT= 128,
} yo_open_flags_t;
typedef enum {
	// Valid on windows only, does not escape arguments
	// Used only to allow cmd /c command. Thanks windows, very cool!
	YO_SPAWN_NOESCAPE,
} yo_spawn_flags_t;
typedef enum {
	YO_PATH_HOME,
	YO_PATH_CONFIG,
	YO_PATH_DATA,
	YO_PATH_CACHE,
	YO_PATH_RUNTIME,
	YO_PATH_CWD,
} yo_path_type_t;
typedef enum {
	YO_PROTO_TCP,
	YO_PROTO_UDP,
} yo_proto_t;
typedef enum {
	YO_TTY_NORMAL,
	YO_TTY_RAW,
} yo_tty_mode_t;

typedef struct {
	enum {
		YO_STAT_REG,
		YO_STAT_DIR,
		YO_STAT_LINK,
		YO_STAT_SOCK,
		YO_STAT_FIFO,
		YO_STAT_CHAR,
		YO_STAT_BLK,
	} type;
	uint32_t mode;
	uint32_t gid;
	uint32_t uid;

	yo_time_t atime, mtime, ctime;

	uint64_t size;
	uint32_t blksize;

	uint64_t inode;
	uint32_t links;
} yo_stat_t;

// A handle roughly equates to a fd (or a windows HANDLE/socket). Such may be an opened file, socket, tty or a pipe.
typedef struct yo_fd *yo_fd_t;

// Creates a handle from an OS-specific FD
// If owned is false, the file won't actually be closed by yo_fd_close()
yo_code_t yo_fd_new(yo_fd_t *pres, uint64_t fd, bool owned);
// Cancels all requests, associated to the handle and releases all resources, used by the `req`
void yo_fd_close(yo_fd_t fd);

// Equivalent to posix's read
yo_code_t yo_read(yo_fd_t fd, char *buff, size_t *pn);
// Equivalent to posix's write
yo_code_t yo_write(yo_fd_t fd, char *buff, size_t *pn);
// Equivalent to posix's sync
yo_code_t yo_sync(yo_fd_t fd);
// Equivalent to posix's stat
yo_code_t yo_stat(yo_fd_t fd, yo_stat_t *buff);

typedef struct yo_tty_raw *yo_tty_raw_t;

// Initializes `tty` to a refrence to `stdin`
yo_code_t yo_tty_in(yo_fd_t *pres);
// Initializes `tty` to a refrence to `stdout`
yo_code_t yo_tty_out(yo_fd_t *pres);
// Initializes `tty` to a refrence to `stderr`
yo_code_t yo_tty_err(yo_fd_t *pres);
// Begins a raw mode of the tty. yo_tty_rawend must be called on yo_tty_raw_t, stored in pres, to end the raw mode
// Make sure to do that, as we are not calling that for you upon exit!
yo_code_t yo_tty_raw(yo_fd_t tty, yo_tty_raw_t *pres);
// Restores the given raw mode to the previous mode of the underlying TTY. This may be another raw mode
yo_code_t yo_tty_rawend(yo_tty_raw_t rawmode);

// Deletes the given file or directory. Fails if directory is not empty
yo_code_t yo_file_remove(const char *path);
// Creates a symbolic link to path at target
yo_code_t yo_file_symlink(const char *src, const char *dst);
// Creates a hard link to the file
yo_code_t yo_file_hardlink(const char *src, const char *dst);
// Reads the given symlink into a malloc'd string
yo_code_t yo_file_readlink(const char *path, char **pres);

// Although on linux, files are blocking, the file functions are async, because they may block for a long time
// (for example, if the file lives on an NFS or FUSE filesystem)

// Equivalent to posix's open
yo_code_t yo_file_open(yo_fd_t *pres, const char *path, yo_open_flags_t flags, int mode);
// A file-specific read function
yo_code_t yo_file_read(yo_fd_t fd, char *buff, size_t *pn, size_t offset);
// A file-specific write function
yo_code_t yo_file_write(yo_fd_t fd, char *buff, size_t *pn, size_t offset);
// Changes the permissions of the given file
yo_code_t yo_file_chmod(yo_fd_t fd, int mode);
// Changes the owner of the given file
yo_code_t yo_file_chown(yo_fd_t fd, int uid, int gid);

typedef struct yo_dir *yo_dir_t;
// Equivalent to posix's mkdir
yo_code_t yo_dir_new(const char *path, int mode);
// Equivalent to posix's opendir
yo_code_t yo_dir_open(yo_dir_t *pres, const char *path);
// Equivalent to posix's readdir
yo_code_t yo_dir_next(yo_dir_t dir, char **pname);
// Equivalent to posix's closedir
void yo_dir_close(yo_dir_t dir);

// Equivalent to connect()
yo_code_t yo_socket_connect(yo_fd_t *pres, yo_proto_t proto, yo_addr_t addr, uint16_t port);
// Equivalent to bind()
yo_code_t yo_socket_bind(yo_fd_t *pres, yo_proto_t proto, yo_addr_t addr, uint16_t port, size_t max_n);
// Equivalent to accept()
yo_code_t yo_socket_accept(yo_fd_t server, yo_fd_t *pres, yo_addr_t *paddr, uint16_t *pport);

typedef struct {
	size_t n;
	yo_addr_t addr[];
} *yo_addrinfo_t;
typedef enum {
	// Resolves only ipv4 (if neither this nor YO_AI_IPV6 are specified, resolves both)
	YO_AI_IPV4 = 1,
	// Resolves only ipv6 (this is mutually-exclusive with IPV4, and this will override IPV4)
	YO_AI_IPV6 = 2,
	// If no IPV6 address was found, but an IPV4 address was, resolves as an ipv6 mapping of the ipv4 address
	YO_AI_IPV4_MAPPED = 4,
	// Resolves to a bindable address - mostly applicable when name is NULL (equivalent to AI_PASSIVE)
	YO_AI_BIND = 8,
	// Resolves only IP addresses - does not make DNS requests (equivalent to AI_NUMERICHOST)
	YO_AI_NODNS = 16,
} yo_addrinfo_flags_t;

// Equivalent to posix's getaddrinfo (with a few simplifications)
yo_code_t yo_dns_getaddrinfo(yo_addrinfo_t *pres, const char *name, yo_addrinfo_flags_t flags);

typedef struct yo_proc *yo_proc_t;

// Equivalent to posix's fork then exec
yo_code_t yo_proc_spawn(
	yo_proc_t *pres, yo_spawn_flags_t flags,
	const char **argv, const char **env, const char *cwd,
	yo_fd_t *pin, yo_fd_t *pout, yo_fd_t *perr
);
// Equivalent to posix's waitpid
// Will free all resources, associated with proc
// psig is set to the signal that terminated the child, or -1 if not terminated by a signal
// pcode is set to the exit code of the app, or -1 if child did not exit with a code
yo_code_t yo_proc_wait(yo_proc_t proc, int *psig, int *pcode);
// Immeditely releases all resources, associated with tracking the process, effectively daemonizing it.
// On unix-like systems, this will initialize a process-wide reaper (if not initialized yet) and put the child in the reaper's list
// Upon our process's exit, as per unix rules, the child will daemonize
yo_code_t yo_proc_disown(yo_proc_t proc);

// Signal handling utilities. NOTE: these won't correlate to signals 1:1, as signals have a stupid amount of historic baggage
// Activating one logical ev signal might activate multiple OS signals, or none at all. Furthermore, the set of signals you can
// receive has been reduced to ones you will want to receive.

// On windows, signals don't exist, so they are "faked" with other facilities.
// This means that some ev signals will never be produced on windows.

// Activates the given signal for receiving. After this call, wait_sig will receive this signal, when generated, as well
// Internally, both this and yo_sig_off use a refcount, so the two must be called in pairs (calling off is optional,
// but it must be called no more times than on has been called per signal)
yo_code_t yo_sig_on(yo_signo_t sig);
// Deactivates the given signal and restores its default semantics. After this call, wait_sig will no longer receiv eit
yo_code_t yo_sig_off(yo_signo_t sig);
// Blocks until the given signal is received.
// NOTE: activating a signal and then not calling sig_wait is equivalent to ignoring it
yo_code_t yo_sig_wait(yo_signo_t *pres);

// Gets a malloc'd string, representing the requested path
yo_code_t yo_getpath(yo_path_type_t type, char **pres);

// Gets an env variable from the current process
yo_code_t yo_env_get(const char *name, char **pres);
// Sets an env variable in the current process (if val is NULL, unsets it)
yo_code_t yo_env_set(const char *name, const char *val);

typedef struct yo_enviter *yo_enviter_t;
// Initializes an iterator of the env variables
yo_enviter_t yo_enviter_new();
// Gets the next env variable from the iterator
yo_code_t yo_enviter_next(yo_enviter_t iter, const char **pres);
void yo_enviter_close(yo_enviter_t iter);

#endif
