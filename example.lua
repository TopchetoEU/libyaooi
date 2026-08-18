local ffi = require "ffi";

local libc = ffi.C;
ffi.cdef [[
	typedef int64_t off_t;
	typedef int ev_code_t;
	typedef int ev_signo_t;

	void *malloc(size_t n);
	void free(void *ptr);
	// int printf(const char *fmt, ...);
]];

if jit.os ~= "Windows" then
	ffi.cdef [[
		int printf(const char *fmt, ...);
	]];
end

local libev = ffi.load(jit.os == "Windows" and "./bin/Windows/libev.dll" or "./bin/Linux/libev.so");
ffi.cdef [[
typedef int ev_code_t;

#line 8 ev/addr.h
typedef enum {
	EV_ADDR_IPV4,
	EV_ADDR_IPV6,
	// TODO: bluetooth maybe?
} ev_addr_type_t;
typedef struct {
	ev_addr_type_t type;
	union {
		uint8_t v4[4];
		uint16_t v6[8];
	};
} ev_addr_t;

// Parses the string to an IP address (ipv4/6 auto-detected)
bool ev_addrparse(const char *str, ev_addr_t *pres);
// Returns true if both addresses are equal
bool ev_addrcmp(ev_addr_t a, ev_addr_t b);


#line 8 ev/time.h
typedef struct {
	int64_t sec;
	uint32_t nsec;
} ev_time_t;

// Adds the two times together
ev_time_t ev_timeadd(ev_time_t a, ev_time_t b);
// Subtracts the two times
ev_time_t ev_timesub(ev_time_t a, ev_time_t b);
// Compares both timestamps, in a strcmp fashion
int ev_timecmp(ev_time_t a, ev_time_t b);
// Converts the time to a millisecond count
int64_t ev_timems(ev_time_t time);

typedef enum {
	EV_CLOCK_REALTIME,
	EV_CLOCK_MONOTIME,
	EV_CLOCK_CPUTIME,
} ev_clock_t;

// Gets the current monotonic time
ev_time_t ev_time(ev_clock_t clock);
// Blocks until the monotonic time is greater than `until`
void ev_timesleep(ev_time_t until);

#line 108 ev/errno.h
// Converts the error code to a human-readable string
const char *ev_strerr(ev_code_t code);

#line 9 ev/queue.h
// Used to deploy a sync workload in an ev-managed thread
typedef int (*ev_worker_t)(void *pargs);

// A structure, keeping track of all pending operations and results
typedef struct ev_queue *ev_queue_t;
// A generic request in the queue
typedef struct ev_req *ev_req_t;

ev_queue_t ev_queue_new();
// Cancels all requests and frees all associated resources
void ev_queue_free(ev_queue_t queue);
// Gets the next completed request in the queue, or blocks until one is available
// - If `pdeadline` is not NULL, limits the block time until the monotonic time is reached
// - If `pdeadline` was reached before a result was pushed, NULL is stored in pres
// - If `pdeadline` is before the current moment, returns immediatly.
// A good way to non-blockingly poll is to pass ev_monotime() to `pdeadline`
ev_code_t ev_queue_poll(ev_queue_t queue, const ev_time_t *pdeadline, ev_req_t *preq, ev_code_t *pcode);

// Inserts a new request in the queue and returns it
ev_req_t ev_req_new(ev_queue_t queue);

// Executes the function in a thread pool returns its value via the request
// The function must return EV_EINTR, if it has been interrupted. This is interpreted as a cancellation
ev_code_t ev_req_exec(ev_req_t req, ev_worker_t worker, void *pargs);

// Cancels the request and removes it from the queue. If it is an IO operation, cancels it in the OS-appropriate way
void ev_req_cancel(ev_req_t req);
// Cancels the request and frees all associated resources
void ev_req_free(ev_req_t req);

#line 15 ev/io.h
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
	// Valid on windows only, does not escape arguments
	// Used only to allow cmd /c command. Thanks windows, very cool!
	EV_SPAWN_NOESCAPE,
} ev_spawn_flags_t;
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
// If owned is false, the file won't actually be closed by ev_fd_close()
ev_code_t ev_fd_new(ev_fd_t *pres, uint64_t fd, bool owned);
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
ev_code_t ev_tty_in(ev_fd_t *pres);
// Initializes `tty` to a refrence to `stdout`
ev_code_t ev_tty_out(ev_fd_t *pres);
// Initializes `tty` to a refrence to `stderr`
ev_code_t ev_tty_err(ev_fd_t *pres);
// Begins a raw mode of the tty. ev_tty_rawend must be called on ev_tty_raw_t, stored in pres, to end the raw mode
// Make sure to do that, as we are not calling that for you upon exit!
ev_code_t ev_tty_raw(ev_fd_t tty, ev_tty_raw_t *pres);
// Restores the given raw mode to the previous mode of the underlying TTY. This may be another raw mode
ev_code_t ev_tty_rawend(ev_tty_raw_t rawmode);

// Deletes the given file or directory. Fails if directory is not empty
ev_code_t ev_file_remove(const char *path);
// Creates a symbolic link to path at target
ev_code_t ev_file_symlink(const char *src, const char *dst);
// Creates a hard link to the file
ev_code_t ev_file_hardlink(const char *src, const char *dst);
// Reads the given symlink into a malloc'd string
ev_code_t ev_file_readlink(const char *path, char **pres);

// Although on linux, files are blocking, the file functions are async, because they may block for a long time
// (for example, if the file lives on an NFS or FUSE filesystem)

// Equivalent to posix's open
ev_code_t ev_file_open(ev_fd_t *pres, const char *path, ev_open_flags_t flags, int mode);
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
ev_code_t ev_dir_new(const char *path, int mode);
// Equivalent to posix's opendir
ev_code_t ev_dir_open(ev_dir_t *pres, const char *path);
// Equivalent to posix's readdir
ev_code_t ev_dir_next(ev_dir_t dir, char **pname);
// Equivalent to posix's closedir
void ev_dir_close(ev_dir_t dir);

// Equivalent to connect()
ev_code_t ev_socket_connect(ev_fd_t *pres, ev_proto_t proto, ev_addr_t addr, uint16_t port);
// Equivalent to bind()
ev_code_t ev_socket_bind(ev_fd_t *pres, ev_proto_t proto, ev_addr_t addr, uint16_t port, size_t max_n);
// Equivalent to accept()
ev_code_t ev_socket_accept(ev_fd_t server, ev_fd_t *pres, ev_addr_t *paddr, uint16_t *pport);

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
ev_code_t ev_dns_getaddrinfo(ev_addrinfo_t *pres, const char *name, ev_addrinfo_flags_t flags);

typedef struct ev_proc *ev_proc_t;

// Equivalent to posix's fork then exec
ev_code_t ev_proc_spawn(
	ev_proc_t *pres, ev_spawn_flags_t flags,
	const char **argv, const char **env, const char *cwd,
	ev_fd_t *pin, ev_fd_t *pout, ev_fd_t *perr
);
// Equivalent to posix's waitpid
// Will free all resources, associated with proc
// psig is set to the signal that terminated the child, or -1 if not terminated by a signal
// pcode is set to the exit code of the app, or -1 if child did not exit with a code
ev_code_t ev_proc_wait(ev_proc_t proc, int *psig, int *pcode);
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
ev_code_t ev_sig_wait(ev_signo_t *pres);

// Gets a malloc'd string, representing the requested path
ev_code_t ev_getpath(ev_path_type_t type, char **pres);

// Gets an env variable from the current process
ev_code_t ev_env_get(const char *name, char **pres);
// Sets an env variable in the current process (if val is NULL, unsets it)
ev_code_t ev_env_set(const char *name, const char *val);

typedef struct ev_enviter *ev_enviter_t;
// Initializes an iterator of the env variables
ev_enviter_t ev_enviter_new();
// Gets the next env variable from the iterator
ev_code_t ev_enviter_next(ev_enviter_t iter, const char **pres);
void ev_enviter_close(ev_enviter_t iter);

#line 10 ev/ioq.h
// Here, queue versions of some functions are presented
// Those that don't have queue equivalents should be used only synchronously

// On linux, some of these are 'technically' blocking-only. However, this design is retarded,
// as network-based FS-es will gladly block for minutes when the underlying connection is lost.
// If that happens, our app would block for minutes as well, rendering the queue system rather useless.
// Underneath, blocking-only ops are run on a thread pool

// Poignant language here used because i am a victim of this "great" design of linux. Thanks, linus!

ev_code_t evq_read(ev_req_t req, ev_fd_t fd, char *buff, size_t *pn);
ev_code_t evq_write(ev_req_t req, ev_fd_t fd, char *buff, size_t *pn);
ev_code_t evq_sync(ev_req_t req, ev_fd_t fd);
ev_code_t evq_stat(ev_req_t req, ev_fd_t fd, ev_stat_t *buff);

ev_code_t evq_file_remove(ev_req_t req, const char *path);
ev_code_t evq_file_symlink(ev_req_t req, const char *src, const char *dst);
ev_code_t evq_file_hardlink(ev_req_t req, const char *src, const char *dst);
ev_code_t evq_file_readlink(ev_req_t req, const char *path, char **pres);

// ev_code_t evq_file_open(ev_req_t req, ev_filelist_t fl, ev_fd_t *pres, const char *path, ev_open_flags_t flags, int mode);
ev_code_t evq_file_read(ev_req_t req, ev_fd_t fd, char *buff, size_t *pn, size_t offset);
ev_code_t evq_file_write(ev_req_t req, ev_fd_t fd, char *buff, size_t *pn, size_t offset);

ev_code_t evq_dir_new(ev_req_t req, const char *path, int mode);
ev_code_t evq_dir_open(ev_req_t req, ev_dir_t *pres, const char *path);
ev_code_t evq_dir_next(ev_req_t req, ev_dir_t dir, char **pname);

ev_code_t evq_socket_connect(ev_req_t req, ev_fd_t *pclient, ev_proto_t proto, ev_addr_t addr, uint16_t port);
ev_code_t evq_socket_accept(ev_req_t req, ev_fd_t server, ev_fd_t *pclient, ev_addr_t *paddr, uint16_t *pport);

ev_code_t evq_dns_getaddrinfo(ev_req_t req, ev_addrinfo_t *pres, const char *name, ev_addrinfo_flags_t flags);

ev_code_t evq_proc_wait(ev_req_t req, ev_proc_t proc, int *psig, int *pcode);

ev_code_t evq_sig_wait(ev_req_t req, ev_signo_t *pres);
]];

-- local curr_tag = 0;
local reqs = {};
local tasks = {};
local sleeps = {};

local queue = libev.ev_queue_new();

local ev = {};

local function qcall(func, cb, ...)
	local req = libev.ev_req_new(queue);

	local code = func(req, ...);
	if code ~= 0 then return nil, ffi.string(libev.ev_strerr(code)), code end

	reqs[tonumber(ffi.cast("size_t", req))] = cb;
	return true;
end

local function parse_ip(str)
	local pres = ffi.new "ev_addr_t[1]";
	assert(libev.ev_addrparse(str, pres), "invalid IP");
	return pres[0];
end

local function pinvoke(handle, ...)
	if type(handle) == "thread" then
		return coroutine.resume(handle, ...);
	elseif type(handle) == "function" then
		return pcall(handle, ...);
	elseif handle == nil then
		return true, "invalid handle";
	else
		return false, "invalid handle";
	end
end
local function invoke(handle, ...)
	if type(handle) == "function" then
		return handle(...);
	end

	local ok, err = pinvoke(handle, ...);
	if not ok then return error(err, 0) end
end

--- @param kind "real" | "mono" | "cpu"
function ev.time(kind)
	local res;
	if kind == "real" then
		res = libev.ev_time(libev.EV_CLOCK_REALTIME);
	elseif kind == "mono" then
		res = libev.ev_time(libev.EV_CLOCK_MONOTIME);
	elseif kind == "cpu" then
		res = libev.ev_time(libev.EV_CLOCK_CPUTIME);
	else
		error "invalid clock type";
	end

	return tonumber(res.sec) + tonumber(res.nsec) / 1000000000;
end

function ev.rawread(cb, fd, ptr, n)
	local pn = ffi.new("size_t[1]", n);

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libev.ev_strerr(code)), code) end
		return invoke(cb, tonumber(pn[0]));
	end

	return qcall(libev.evq_read, handle, fd, ptr, pn);
end
function ev.rawwrite(cb, fd, ptr, n)
	local pn = ffi.new("size_t[1]", n);

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libev.ev_strerr(code)), code) end
		return invoke(cb, tonumber(pn[0]));
	end

	return qcall(libev.evq_write, handle, fd, ptr, pn);
end
function ev.read(cb, sock, n)
	local buff = ffi.new("char[?]", n);

	return ev.rawread(function (n, err)
		if not n then return invoke(cb, n, err) end
		return invoke(cb, ffi.string(buff, n));
	end, sock, buff, n);
end
function ev.write(cb, sock, str)
	local buff = ffi.new("char[?]", #str);
	ffi.copy(buff, str, #str);

	return ev.rawwrite(function (n, err)
		if not n then return invoke(cb, n, err) end
		return invoke(cb, n);
	end, sock, buff, #str);
end
function ev.sync(cb, fd)
	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libev.ev_strerr(code)), code) end
		return invoke(cb, true);
	end

	return qcall(libev.evq_stat, handle, fd);
end
function ev.stat(cb, fd)
	local pbuff = ffi.new "ev_stat_t[1]";

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libev.ev_strerr(code)), code) end
		return invoke(cb, pbuff[0]);
	end

	return qcall(libev.evq_stat, handle, fd, pbuff);
end
ev.close = libev.ev_fd_close;

function ev.file_open(cb, path, flags, mode)
	local pres = ffi.new "ev_fd_t[1]";

	local code = libev.ev_file_open(pres, path, flags, assert(tonumber(mode, 8)));
	if code ~= 0 then return invoke(cb, nil, ffi.string(libev.ev_strerr(code)), code) end

	return pres[0];
end
function ev.file_rawread(cb, fd, offset, ptr, n)
	local pn = ffi.new("size_t[1]", n);

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libev.ev_strerr(code)), code) end
		return invoke(cb, pn[0]);
	end

	return qcall(libev.evq_file_read, handle, fd, ptr, pn, offset);
end
function ev.file_rawwrite(cb, fd, offset, ptr, n)
	local pn = ffi.new("size_t[1]", n);

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libev.ev_strerr(code)), code) end
		return invoke(cb, pn[0]);
	end

	return qcall(libev.evq_file_write, handle, fd, ptr, pn, offset);
end
function ev.file_read(cb, fd, offset, n)
	local buff = ffi.new("char[?]", n);

	return ev.file_rawread(function (n, err)
		if not n then return invoke(cb, n, err) end
		return invoke(cb, ffi.string(buff, n));
	end, fd, offset, buff, n);
end
function ev.file_write(cb, fd, offset, str)
	local buff = ffi.new("char[?]", #str);
	ffi.copy(buff, str, #str);

	return ev.file_rawwrite(function (n, err)
		if not n then return invoke(cb, n, err) end
		return invoke(cb, n);
	end, fd, offset, buff, #str);
end

function ev.dir_new(cb, path, mode)
	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libev.ev_strerr(code)), code) end
		return invoke(cb, true);
	end

	return qcall(libev.evq_dir_new, handle, path, assert(tonumber(mode or 777, 8)));
end
function ev.dir_open(cb, path)
	local pres = ffi.new "ev_dir_t[1]";

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libev.ev_strerr(code)), code) end
		return invoke(cb, pres[0]);
	end

	return qcall(libev.evq_dir_open, handle, pres, path);
end
function ev.dir_next(cb, dir)
	local pname = ffi.new "char*[1]";

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libev.ev_strerr(code)), code) end
		return invoke(cb, pname[0]);
	end

	return qcall(libev.evq_dir_next, handle, dir, pname);
end
ev.dir_close = libev.ev_dir_close;

function ev.proc_spawn(opts)
	local pres = ffi.new "ev_proc_t[1]";

	local pin = opts.stdin and ffi.new "ev_fd_t[1]" or nil;
	local pout = opts.stdout and ffi.new "ev_fd_t[1]" or nil;
	local perr = opts.stderr and ffi.new "ev_fd_t[1]" or nil;

	local function strdup(str)
		local res = libc.malloc(#str + 1);
		ffi.copy(res, str);
		return res;
	end

	local argv = ffi.cast("const char**", libc.malloc(ffi.sizeof "const char*" * (#opts.argv + 1)));
	for i = 1, #opts.argv do
		argv[i - 1] = strdup(opts.argv[i]);
	end
	argv[#opts.argv] = nil;

	local env_key_n = 0;

	for _ in pairs(opts.env) do
		env_key_n = env_key_n + 1;
	end

	local env = ffi.cast("const char**", libc.malloc(ffi.sizeof "const char**" * (#opts.env + env_key_n + 1)));

	for i = 1, #opts.env do
		env[i - 1] = strdup(opts.env[i][1] .. "=" .. opts.env[i][2]);
	end

	local i = 0;
	for k, v in pairs(opts.env) do
		env[#opts.env + i] = strdup(k .. v);
		i = i + 1;
	end

	env[#opts.env + env_key_n] = nil;

	local code = libev.ev_proc_spawn(pres, 0, argv, env, opts.cwd, pin, pout, perr);
	if code ~= 0 then return nil, ffi.string(libev.ev_strerr(code)), code end
	return pres[0], pin and pin[0], pout and pout[0], perr and perr[0];
end
function ev.proc_wait(cb, proc)
	local pcode = ffi.new "int[1]";
	local psig = ffi.new "int[1]";

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libev.ev_strerr(code)), code) end
		return invoke(cb, tonumber(pcode[0]), tonumber(psig[0]));
	end

	return qcall(libev.evq_proc_wait, handle, proc, pcode, psig);
end
ev.proc_disown = libev.ev_proc_disown;

function ev.socket_connect(cb, addr, port, type)
	local itype;
	local pres = ffi.new "ev_fd_t[1]";

	if type == "tcp" then
		itype = 0;
	elseif type == "udp" then
		itype = 1;
	else
		error "invalid type";
	end

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libev.ev_strerr(code)), code) end
		return invoke(cb, pres[0]);
	end

	return qcall(libev.evq_socket_connect, handle, pres, itype, parse_ip(addr), port);
end
function ev.socket_accept(cb, server)
	local pres = ffi.new "ev_fd_t[1]";
	local paddr = ffi.new "ev_addr_t[1]";
	local pport = ffi.new "uint16_t[1]";

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libev.ev_strerr(code)), code) end
		return invoke(cb, pres[0], paddr[0], pport[0]);
	end

	return qcall(libev.evq_socket_accept, handle, server, pres, paddr, pport);
end
function ev.socket_bind(addr, port, type, max_n)
	local itype;
	local pres = ffi.new "ev_fd_t[1]";

	if type == "tcp" then
		itype = 0;
	elseif type == "udp" then
		itype = 1;
	else
		error "invalid type";
	end

	local code = libev.ev_socket_bind(pres, parse_ip(addr), itype, port, max_n);
	if code ~= 0 then return nil, ffi.string(libev.ev_strerr(code)), code end

	return pres[0];
end

function ev.dns_getaddrinfo(cb, name, flags)
	local pres = ffi.new "ev_addrinfo_t[1]";

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libev.ev_strerr(code)), code) end

		local res = {};

		for i = 1, tonumber(pres[0].n) do
			local addr = pres[0].addr[i - 1];

			if addr.type == 0 then
				table.insert(res, ("%d.%d.%d.%d"):format(addr.v4[0], addr.v4[1], addr.v4[2], addr.v4[3]));
			else
				table.insert(res, ("%x:%x:%x:%x:%x:%x:%x:%x"):format(
					addr.v6[0], addr.v6[1],
					addr.v6[2], addr.v6[3],
					addr.v6[4], addr.v6[5],
					addr.v6[6], addr.v6[7]
				));
			end
		end

		return invoke(cb, res);
	end

	return qcall(libev.evq_dns_getaddrinfo, handle, pres, name, flags);
end

function ev.sig_on(signo)
	local code = libev.ev_sig_on(signo);
	if code ~= 0 then return nil, ffi.string(libev.ev_strerr(code)), code end
end
function ev.sig_off(signo)
	local code = libev.ev_sig_off(signo);
	if code ~= 0 then return nil, ffi.string(libev.ev_strerr(code)), code end
end
function ev.sig_wait(cb)
	local pres = ffi.new "ev_signo_t[1]";

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libev.ev_strerr(code)), code) end
		return invoke(cb, tonumber(pres[0]));
	end

	return qcall(libev.evq_sig_wait, handle, pres);
end

function ev.getpath(type)
	local pres = ffi.new "char*[1]";
	local code = libev.ev_getpath(pres, type);

	if code ~= 0 then return nil, ffi.string(libev.ev_strerr(code)), code end

	local res = ffi.string(pres[0]);
	libc.free(pres[0]);
	return res;
end
function ev.env_get(name)
	local pres = ffi.new "char*[1]";
	local code = libev.ev_env_get(name, pres);

	if code ~= 0 then return nil, ffi.string(libev.ev_strerr(code)), code end

	local res = ffi.string(pres[0]);
	libc.free(pres[0]);
	return res;
end
function ev.env_set(name, val)
	local code = libev.ev_env_set(name, val);
	if code ~= 0 then return nil, ffi.string(libev.ev_strerr(code)), code end
	return true;
end

ev.enviter_new = libev.ev_enviter_new;
ev.enviter_close = libev.ev_enviter_close;
function ev.enviter_next(iter)
	local pres = ffi.new "const char *[1]";
	local code = libev.ev_enviter_next(iter, pres);
	if code ~= 0 then return nil, ffi.string(libev.ev_strerr(code)), code end

	if pres[0] == ffi.cast("void*", 0) then
		return nil;
	else
		return ffi.string(pres[0]);
	end
end

local function syncify(func)
	return function (...)
		local ok, err = func(coroutine.running(), ...);
		if not ok then return nil, err end
		return coroutine.yield();
	end
end

local evs = {
	rawread = syncify(ev.rawread),
	rawwrite = syncify(ev.rawwrite),
	read = syncify(ev.read),
	write = syncify(ev.write),
	sync = syncify(ev.sync),
	stat = syncify(ev.stat),
	close = ev.close,

	file_open = syncify(ev.file_open),
	file_rawread = syncify(ev.file_rawread),
	file_rawwrite = syncify(ev.file_rawwrite),
	file_read = syncify(ev.file_read),
	file_write = syncify(ev.file_write),

	dir_new = syncify(ev.dir_new),
	dir_open = syncify(ev.dir_open),
	dir_read = syncify(ev.dir_next),
	dir_close = ev.dir_close,

	socket_connect = syncify(ev.socket_connect),
	socket_accept = syncify(ev.socket_accept),
	socket_bind = ev.socket_bind,

	sig_on = ev.sig_on,
	sig_off = ev.sig_off,
	sig_wait = syncify(ev.sig_wait),

	proc_spawn = ev.proc_spawn,
	proc_wait = syncify(ev.proc_wait),

	dns_getaddrinfo = syncify(ev.dns_getaddrinfo),
	getpath = ev.getpath,

	env_get = ev.env_get,
	env_set = ev.env_set,
};

local function run()
	while true do
		local curr = ev.time "mono";

		-- NOTE: this can be implemented as a sorted list, which would be MUCH faster for lots of concurrent sleeps, this is just the simplest logic
		for i = #sleeps, 1, -1 do
			if sleeps[i].time <= curr then
				table.insert(tasks, sleeps[i].task);
				table.remove(sleeps, i);
			end
		end

		while true do
			local task = table.remove(tasks, 1);
			if not task then break end

			local ok, err = pinvoke(task);
			if not ok then return nil, err end
		end

		local timeout;
		for i = #sleeps, 1, -1 do
			if not timeout or timeout > sleeps[i].time then
				timeout = sleeps[i].time;
			end
		end

		if
			not timeout and
			not next(reqs) and
			#tasks == 0
		then return true end

		local pdeadline = nil;
		if timeout then
			pdeadline = ffi.new "ev_time_t[1]";
			pdeadline[0].sec = timeout - timeout % 1;
			pdeadline[0].nsec = (timeout % 1) * 1000000000;
		end

		local preq = ffi.new "ev_req_t[1]";
		local perr = ffi.new "int[1]";

		local code = libev.ev_queue_poll(queue, pdeadline, preq, perr);
		if code == 0 then
			local ireq = assert(tonumber(ffi.cast("size_t", preq[0])));
			local cb = reqs[ireq];
			reqs[ireq] = nil;

			libev.ev_req_free(preq[0]);

			local ok, err = pinvoke(cb, perr[0]);
			if not ok then return nil, err end
		elseif code ~= -110 --[[ ETIMEDOUT ]] then
			error(code);
		end
	end
end

local function sleep_until(time)
	local cb = coroutine.running();
	table.insert(sleeps, { time = time, task = cb });

	return coroutine.yield();
end
local function sleep(secs)
	return sleep_until(secs + ev.monotime());
end

--- @param func fun(...)
local function fork(func, ...)
	local thread = coroutine.create(function (...)
		local ok, err = xpcall(func, debug.traceback, ...);
		if not ok then error(err, 0) end
	end);

	local ok, err = coroutine.resume(thread, ...);
	if not ok then error(err, 0) end
end

local function interrupt()
	table.insert(tasks, (coroutine.running()));
	return coroutine.yield();
end

local function open_tcp(name, port)
	local err;
	for _, data in ipairs(assert(evs.dns_getaddrinfo(name, 0))) do
		local res;
		res, err = evs.socket_connect(data, port, "tcp");
		if res then return res end
	end

	return nil, err or "host unreachable";
end

local pstderr = ffi.new "ev_fd_t[1]";
libev.ev_tty_err(pstderr);
local stderr = pstderr[0];

local function netcat(url)
	local sock = assert(open_tcp(url, 80));

	assert(evs.write(sock, "GET / HTTP/1.1\r\nHost: " .. url .. "\r\nUser-Agent: example/0.1\r\nConnection: close\r\n\r\n"));
	while true do
		local res = assert(evs.read(sock, 100));
		if #res == 0 then break end

		-- io.stderr:write(res);
		assert(evs.write(stderr, res));
	end
	evs.close(sock);
end

fork(function ()
	evs.sig_on(0);

	while true do
		print "WAIT";
		local sig = evs.sig_wait();
		print("SIGNAL", sig);
		if sig == 0 then
			error "exit";
		end
	end
end);

-- fork(netcat, "www.topcheto.eu");
fork(netcat, "www.example.org");
fork(netcat, "www.example.com");

fork(function ()
	local base = ev.time "mono";

	for i = 1, 50 do
		sleep_until(base + i * .01);
		print("====================> MS " .. i * 10);
	end
end);

fork(function ()
	local proc, proc_in, proc_out = assert(evs.proc_spawn {
		stdin = true,
		stdout = true,
		argv = ffi.os == "Windows" and { "./cat.exe", "-" } or { "/bin/sort" },
		env = {},
	});

	fork(function ()
		assert(evs.write(proc_in, "The quick brown fox jumped over the red dog\n"));
		assert(evs.write(proc_in, "Lorem ipsum dolor sit amet, consectetur adipiscing elit.\n"));
		assert(evs.write(proc_in, "Integer consectetur mi a feugiat tempor.\n"));
		assert(evs.write(proc_in, "Cras tincidunt diam at libero lacinia, ac fringilla metus malesuada.\n"));
		evs.close(proc_in);
	end);

	fork(function ()
		while true do
			local buff = assert(evs.read(proc_out, 1024));
			if #buff == 0 then break end
			io.stderr:write(buff);
		end
		evs.close(proc_out);

		print("EXIT CODE", assert(evs.proc_wait(proc)));
	end);
end);

fork(function ()
	for pair in ev.enviter_next, ev.enviter_new() do
		print(pair:match "(.-)=(.*)");
		interrupt();
	end
end);

assert(run());
libev.ev_queue_free(queue);
