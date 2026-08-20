--[[
Copyright 2026 TopchetoEU

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the “Software”), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
]]

local ffi = require "ffi";

local libc = ffi.C;
ffi.cdef [[
	typedef int64_t off_t;
	typedef int yo_code_t;
	typedef int yo_signo_t;

	void *malloc(size_t n);
	void free(void *ptr);
	// int printf(const char *fmt, ...);
]];

if jit.os ~= "Windows" then
	ffi.cdef [[
		int printf(const char *fmt, ...);
	]];
end

local libyaooi = ffi.load(jit.os == "Windows" and "./bin/Windows/libyaooi.dll" or "./bin/Linux/libyaooi.so");
ffi.cdef [[
typedef int yo_code_t;

#line 8 ev/addr.h
typedef enum {
	YO_ADDR_IPV4,
	YO_ADDR_IPV6,
	// TODO: bluetooth maybe?
} yo_addr_type_t;
typedef struct {
	yo_addr_type_t type;
	union {
		uint8_t v4[4];
		uint16_t v6[8];
	};
} yo_addr_t;

// Parses the string to an IP address (ipv4/6 auto-detected)
bool yo_addrparse(const char *str, yo_addr_t *pres);
// Returns true if both addresses are equal
bool yo_addrcmp(yo_addr_t a, yo_addr_t b);


#line 8 ev/time.h
typedef struct {
	int64_t sec;
	uint32_t nsec;
} yo_time_t;

// Adds the two times together
yo_time_t yo_timeadd(yo_time_t a, yo_time_t b);
// Subtracts the two times
yo_time_t yo_timesub(yo_time_t a, yo_time_t b);
// Compares both timestamps, in a strcmp fashion
int yo_timecmp(yo_time_t a, yo_time_t b);
// Converts the time to a millisecond count
int64_t yo_timems(yo_time_t time);

typedef enum {
	YO_CLOCK_REAL,
	YO_CLOCK_MONO,
	YO_CLOCK_CPU,
} yo_clock_t;

// Gets the current monotonic time
yo_time_t yo_time(yo_clock_t clock);
// Blocks until the monotonic time is greater than `until`
void yo_timesleep(yo_time_t until);

#line 108 ev/errno.h
// Converts the error code to a human-readable string
const char *yo_strerr(yo_code_t code);

#line 9 ev/queue.h
// Used to deploy a sync workload in an ev-managed thread
typedef int (*yo_worker_t)(void *pargs);

// A structure, keeping track of all pending operations and results
typedef struct yo_queue *yo_queue_t;
// A generic request in the queue
typedef struct yo_req *yo_req_t;

yo_queue_t yo_queue_new();
// Cancels all requests and frees all associated resources
void yo_queue_free(yo_queue_t queue);
// Gets the next completed request in the queue, or blocks until one is available
// - If `pdeadline` is not NULL, limits the block time until the monotonic time is reached
// - If `pdeadline` was reached before a result was pushed, NULL is stored in pres
// - If `pdeadline` is before the current moment, returns immediatly.
// A good way to non-blockingly poll is to pass yo_monotime() to `pdeadline`
yo_code_t yo_queue_poll(yo_queue_t queue, const yo_time_t *pdeadline, yo_req_t *preq, yo_code_t *pcode);

// Inserts a new request in the queue and returns it
yo_req_t yo_req_new(yo_queue_t queue);

// Executes the function in a thread pool returns its value via the request
// The function must return YO_EINTR, if it has been interrupted. This is interpreted as a cancellation
yo_code_t yo_req_exec(yo_req_t req, yo_worker_t worker, void *pargs);

// Cancels the request and removes it from the queue. If it is an IO operation, cancels it in the OS-appropriate way
void yo_req_cancel(yo_req_t req);
// Cancels the request and frees all associated resources
void yo_req_free(yo_req_t req);

#line 15 ev/io.h
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

#line 10 ev/ioq.h
// Here, queue versions of some functions are presented
// Those that don't have queue equivalents should be used only synchronously

// On linux, some of these are 'technically' blocking-only. However, this design is retarded,
// as network-based FS-es will gladly block for minutes when the underlying connection is lost.
// If that happens, our app would block for minutes as well, rendering the queue system rather useless.
// Underneath, blocking-only ops are run on a thread pool

// Poignant language here used because i am a victim of this "great" design of linux. Thanks, linus!

yo_code_t yoa_read(yo_req_t req, yo_fd_t fd, char *buff, size_t *pn);
yo_code_t yoa_write(yo_req_t req, yo_fd_t fd, char *buff, size_t *pn);
yo_code_t yoa_sync(yo_req_t req, yo_fd_t fd);
yo_code_t yoa_stat(yo_req_t req, yo_fd_t fd, yo_stat_t *buff);

yo_code_t yoa_file_remove(yo_req_t req, const char *path);
yo_code_t yoa_file_symlink(yo_req_t req, const char *src, const char *dst);
yo_code_t yoa_file_hardlink(yo_req_t req, const char *src, const char *dst);
yo_code_t yoa_file_readlink(yo_req_t req, const char *path, char **pres);

// yo_code_t yoa_file_open(yo_req_t req, yo_filelist_t fl, yo_fd_t *pres, const char *path, yo_open_flags_t flags, int mode);
yo_code_t yoa_file_read(yo_req_t req, yo_fd_t fd, char *buff, size_t *pn, size_t offset);
yo_code_t yoa_file_write(yo_req_t req, yo_fd_t fd, char *buff, size_t *pn, size_t offset);

yo_code_t yoa_dir_new(yo_req_t req, const char *path, int mode);
yo_code_t yoa_dir_open(yo_req_t req, yo_dir_t *pres, const char *path);
yo_code_t yoa_dir_next(yo_req_t req, yo_dir_t dir, char **pname);

yo_code_t yoa_socket_connect(yo_req_t req, yo_fd_t *pclient, yo_proto_t proto, yo_addr_t addr, uint16_t port);
yo_code_t yoa_socket_accept(yo_req_t req, yo_fd_t server, yo_fd_t *pclient, yo_addr_t *paddr, uint16_t *pport);

yo_code_t yoa_dns_getaddrinfo(yo_req_t req, yo_addrinfo_t *pres, const char *name, yo_addrinfo_flags_t flags);

yo_code_t yoa_proc_wait(yo_req_t req, yo_proc_t proc, int *psig, int *pcode);

yo_code_t yoa_sig_wait(yo_req_t req, yo_signo_t *pres);
]];

-- local curr_tag = 0;
local reqs = {};
local tasks = {};
local sleeps = {};

local queue = libyaooi.yo_queue_new();

local yo = {};

local function qcall(func, cb, ...)
	local req = libyaooi.yo_req_new(queue);

	local code = func(req, ...);
	if code ~= 0 then return nil, ffi.string(libyaooi.yo_strerr(code)), code end

	reqs[tonumber(ffi.cast("size_t", req))] = cb;
	return true;
end

local function parse_ip(str)
	local pres = ffi.new "yo_addr_t[1]";
	assert(libyaooi.yo_addrparse(str, pres), "invalid IP");
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
function yo.time(kind)
	local res;
	if kind == "real" then
		res = libyaooi.yo_time(libyaooi.YO_CLOCK_REAL);
	elseif kind == "mono" then
		res = libyaooi.yo_time(libyaooi.YO_CLOCK_MONO);
	elseif kind == "cpu" then
		res = libyaooi.yo_time(libyaooi.YO_CLOCK_CPU);
	else
		error "invalid clock type";
	end

	return tonumber(res.sec) + tonumber(res.nsec) / 1000000000;
end

function yo.rawread(cb, fd, ptr, n)
	local pn = ffi.new("size_t[1]", n);

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libyaooi.yo_strerr(code)), code) end
		return invoke(cb, tonumber(pn[0]));
	end

	return qcall(libyaooi.yoa_read, handle, fd, ptr, pn);
end
function yo.rawwrite(cb, fd, ptr, n)
	local pn = ffi.new("size_t[1]", n);

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libyaooi.yo_strerr(code)), code) end
		return invoke(cb, tonumber(pn[0]));
	end

	return qcall(libyaooi.yoa_write, handle, fd, ptr, pn);
end
function yo.read(cb, sock, n)
	local buff = ffi.new("char[?]", n);

	return yo.rawread(function (n, err)
		if not n then return invoke(cb, n, err) end
		return invoke(cb, ffi.string(buff, n));
	end, sock, buff, n);
end
function yo.write(cb, sock, str)
	local buff = ffi.new("char[?]", #str);
	ffi.copy(buff, str, #str);

	return yo.rawwrite(function (n, err)
		if not n then return invoke(cb, n, err) end
		return invoke(cb, n);
	end, sock, buff, #str);
end
function yo.sync(cb, fd)
	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libyaooi.yo_strerr(code)), code) end
		return invoke(cb, true);
	end

	return qcall(libyaooi.yoa_stat, handle, fd);
end
function yo.stat(cb, fd)
	local pbuff = ffi.new "yo_stat_t[1]";

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libyaooi.yo_strerr(code)), code) end
		return invoke(cb, pbuff[0]);
	end

	return qcall(libyaooi.yoa_stat, handle, fd, pbuff);
end
yo.close = libyaooi.yo_fd_close;

function yo.file_open(cb, path, flags, mode)
	local pres = ffi.new "yo_fd_t[1]";

	local code = libyaooi.yo_file_open(pres, path, flags, assert(tonumber(mode, 8)));
	if code ~= 0 then return invoke(cb, nil, ffi.string(libyaooi.yo_strerr(code)), code) end

	return pres[0];
end
function yo.file_rawread(cb, fd, offset, ptr, n)
	local pn = ffi.new("size_t[1]", n);

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libyaooi.yo_strerr(code)), code) end
		return invoke(cb, pn[0]);
	end

	return qcall(libyaooi.yoa_file_read, handle, fd, ptr, pn, offset);
end
function yo.file_rawwrite(cb, fd, offset, ptr, n)
	local pn = ffi.new("size_t[1]", n);

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libyaooi.yo_strerr(code)), code) end
		return invoke(cb, pn[0]);
	end

	return qcall(libyaooi.yoa_file_write, handle, fd, ptr, pn, offset);
end
function yo.file_read(cb, fd, offset, n)
	local buff = ffi.new("char[?]", n);

	return yo.file_rawread(function (n, err)
		if not n then return invoke(cb, n, err) end
		return invoke(cb, ffi.string(buff, n));
	end, fd, offset, buff, n);
end
function yo.file_write(cb, fd, offset, str)
	local buff = ffi.new("char[?]", #str);
	ffi.copy(buff, str, #str);

	return yo.file_rawwrite(function (n, err)
		if not n then return invoke(cb, n, err) end
		return invoke(cb, n);
	end, fd, offset, buff, #str);
end

function yo.dir_new(cb, path, mode)
	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libyaooi.yo_strerr(code)), code) end
		return invoke(cb, true);
	end

	return qcall(libyaooi.yoa_dir_new, handle, path, assert(tonumber(mode or 777, 8)));
end
function yo.dir_open(cb, path)
	local pres = ffi.new "yo_dir_t[1]";

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libyaooi.yo_strerr(code)), code) end
		return invoke(cb, pres[0]);
	end

	return qcall(libyaooi.yoa_dir_open, handle, pres, path);
end
function yo.dir_next(cb, dir)
	local pname = ffi.new "char*[1]";

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libyaooi.yo_strerr(code)), code) end
		return invoke(cb, pname[0]);
	end

	return qcall(libyaooi.yoa_dir_next, handle, dir, pname);
end
yo.dir_close = libyaooi.yo_dir_close;

function yo.proc_spawn(opts)
	local pres = ffi.new "yo_proc_t[1]";

	local pin = opts.stdin and ffi.new "yo_fd_t[1]" or nil;
	local pout = opts.stdout and ffi.new "yo_fd_t[1]" or nil;
	local perr = opts.stderr and ffi.new "yo_fd_t[1]" or nil;

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

	local code = libyaooi.yo_proc_spawn(pres, 0, argv, env, opts.cwd, pin, pout, perr);
	if code ~= 0 then return nil, ffi.string(libyaooi.yo_strerr(code)), code end
	return pres[0], pin and pin[0], pout and pout[0], perr and perr[0];
end
function yo.proc_wait(cb, proc)
	local pcode = ffi.new "int[1]";
	local psig = ffi.new "int[1]";

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libyaooi.yo_strerr(code)), code) end
		return invoke(cb, tonumber(pcode[0]), tonumber(psig[0]));
	end

	return qcall(libyaooi.yoa_proc_wait, handle, proc, pcode, psig);
end
yo.proc_disown = libyaooi.yo_proc_disown;

function yo.socket_connect(cb, addr, port, type)
	local itype;
	local pres = ffi.new "yo_fd_t[1]";

	if type == "tcp" then
		itype = 0;
	elseif type == "udp" then
		itype = 1;
	else
		error "invalid type";
	end

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libyaooi.yo_strerr(code)), code) end
		return invoke(cb, pres[0]);
	end

	return qcall(libyaooi.yoa_socket_connect, handle, pres, itype, parse_ip(addr), port);
end
function yo.socket_accept(cb, server)
	local pres = ffi.new "yo_fd_t[1]";
	local paddr = ffi.new "yo_addr_t[1]";
	local pport = ffi.new "uint16_t[1]";

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libyaooi.yo_strerr(code)), code) end
		return invoke(cb, pres[0], paddr[0], pport[0]);
	end

	return qcall(libyaooi.yoa_socket_accept, handle, server, pres, paddr, pport);
end
function yo.socket_bind(addr, port, type, max_n)
	local itype;
	local pres = ffi.new "yo_fd_t[1]";

	if type == "tcp" then
		itype = 0;
	elseif type == "udp" then
		itype = 1;
	else
		error "invalid type";
	end

	local code = libyaooi.yo_socket_bind(pres, parse_ip(addr), itype, port, max_n);
	if code ~= 0 then return nil, ffi.string(libyaooi.yo_strerr(code)), code end

	return pres[0];
end

function yo.dns_getaddrinfo(cb, name, flags)
	local pres = ffi.new "yo_addrinfo_t[1]";

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libyaooi.yo_strerr(code)), code) end

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

	return qcall(libyaooi.yoa_dns_getaddrinfo, handle, pres, name, flags);
end

function yo.sig_on(signo)
	local code = libyaooi.yo_sig_on(signo);
	if code ~= 0 then return nil, ffi.string(libyaooi.yo_strerr(code)), code end
end
function yo.sig_off(signo)
	local code = libyaooi.yo_sig_off(signo);
	if code ~= 0 then return nil, ffi.string(libyaooi.yo_strerr(code)), code end
end
function yo.sig_wait(cb)
	local pres = ffi.new "yo_signo_t[1]";

	local function handle(code)
		if code ~= 0 then return invoke(cb, nil, ffi.string(libyaooi.yo_strerr(code)), code) end
		return invoke(cb, tonumber(pres[0]));
	end

	return qcall(libyaooi.yoa_sig_wait, handle, pres);
end

function yo.getpath(type)
	local pres = ffi.new "char*[1]";
	local code = libyaooi.yo_getpath(pres, type);

	if code ~= 0 then return nil, ffi.string(libyaooi.yo_strerr(code)), code end

	local res = ffi.string(pres[0]);
	libc.free(pres[0]);
	return res;
end
function yo.env_get(name)
	local pres = ffi.new "char*[1]";
	local code = libyaooi.yo_env_get(name, pres);

	if code ~= 0 then return nil, ffi.string(libyaooi.yo_strerr(code)), code end

	local res = ffi.string(pres[0]);
	libc.free(pres[0]);
	return res;
end
function yo.env_set(name, val)
	local code = libyaooi.yo_env_set(name, val);
	if code ~= 0 then return nil, ffi.string(libyaooi.yo_strerr(code)), code end
	return true;
end

yo.enviter_new = libyaooi.yo_enviter_new;
yo.enviter_close = libyaooi.yo_enviter_close;
function yo.enviter_next(iter)
	local pres = ffi.new "const char *[1]";
	local code = libyaooi.yo_enviter_next(iter, pres);
	if code ~= 0 then return nil, ffi.string(libyaooi.yo_strerr(code)), code end

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

local yos = {
	rawread = syncify(yo.rawread),
	rawwrite = syncify(yo.rawwrite),
	read = syncify(yo.read),
	write = syncify(yo.write),
	sync = syncify(yo.sync),
	stat = syncify(yo.stat),
	close = yo.close,

	file_open = syncify(yo.file_open),
	file_rawread = syncify(yo.file_rawread),
	file_rawwrite = syncify(yo.file_rawwrite),
	file_read = syncify(yo.file_read),
	file_write = syncify(yo.file_write),

	dir_new = syncify(yo.dir_new),
	dir_open = syncify(yo.dir_open),
	dir_read = syncify(yo.dir_next),
	dir_close = yo.dir_close,

	socket_connect = syncify(yo.socket_connect),
	socket_accept = syncify(yo.socket_accept),
	socket_bind = yo.socket_bind,

	sig_on = yo.sig_on,
	sig_off = yo.sig_off,
	sig_wait = syncify(yo.sig_wait),

	proc_spawn = yo.proc_spawn,
	proc_wait = syncify(yo.proc_wait),

	dns_getaddrinfo = syncify(yo.dns_getaddrinfo),
	getpath = yo.getpath,

	env_get = yo.env_get,
	env_set = yo.env_set,
};

local function run()
	while true do
		local curr = yo.time "mono";

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
			pdeadline = ffi.new "yo_time_t[1]";
			pdeadline[0].sec = timeout - timeout % 1;
			pdeadline[0].nsec = (timeout % 1) * 1000000000;
		end

		local preq = ffi.new "yo_req_t[1]";
		local perr = ffi.new "int[1]";

		local code = libyaooi.yo_queue_poll(queue, pdeadline, preq, perr);
		if code == 0 then
			local ireq = assert(tonumber(ffi.cast("size_t", preq[0])));
			local cb = reqs[ireq];
			reqs[ireq] = nil;

			libyaooi.yo_req_free(preq[0]);

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
	return sleep_until(secs + yo.monotime());
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
	for _, data in ipairs(assert(yos.dns_getaddrinfo(name, 0))) do
		local res;
		res, err = yos.socket_connect(data, port, "tcp");
		if res then return res end
	end

	return nil, err or "host unreachable";
end

local pstderr = ffi.new "yo_fd_t[1]";
libyaooi.yo_tty_err(pstderr);
local stderr = pstderr[0];

local function netcat(url)
	local sock = assert(open_tcp(url, 80));

	assert(yos.write(sock, "GET / HTTP/1.1\r\nHost: " .. url .. "\r\nUser-Agent: example/0.1\r\nConnection: close\r\n\r\n"));
	while true do
		local res = assert(yos.read(sock, 100));
		if #res == 0 then break end

		-- io.stderr:write(res);
		assert(yos.write(stderr, res));
	end
	yos.close(sock);
end

fork(function ()
	yos.sig_on(0);

	while true do
		print "WAIT";
		local sig = yos.sig_wait();
		print("SIGNAL", sig);
		if sig == 0 then
			error "exit";
		end
	end
end);

fork(netcat, "www.topcheto.eu");
fork(netcat, "www.example.org");

fork(function ()
	local base = yo.time "mono";

	for i = 1, 50 do
		sleep_until(base + i * .01);
		print("====================> MS " .. i * 10);
	end
end);

fork(function ()
	local proc, proc_in, proc_out = assert(yos.proc_spawn {
		stdin = true,
		stdout = true,
		argv = ffi.os == "Windows" and { "./cat.exe", "-" } or { "/bin/sort" },
		env = {},
	});

	fork(function ()
		assert(yos.write(proc_in, "The quick brown fox jumped over the red dog\n"));
		assert(yos.write(proc_in, "Lorem ipsum dolor sit amet, consectetur adipiscing elit.\n"));
		assert(yos.write(proc_in, "Integer consectetur mi a feugiat tempor.\n"));
		assert(yos.write(proc_in, "Cras tincidunt diam at libero lacinia, ac fringilla metus malesuada.\n"));
		yos.close(proc_in);
	end);

	fork(function ()
		while true do
			local buff = assert(yos.read(proc_out, 1024));
			if #buff == 0 then break end
			io.stderr:write(buff);
		end
		yos.close(proc_out);

		print("EXIT CODE", assert(yos.proc_wait(proc)));
	end);
end);

fork(function ()
	for pair in yo.enviter_next, yo.enviter_new() do
		print(pair:match "(.-)=(.*)");
		interrupt();
	end
end);

assert(run());
libyaooi.yo_queue_free(queue);
