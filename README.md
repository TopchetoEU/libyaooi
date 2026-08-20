libyaooi (Yet Another OS Operations Interface, you pervert) is a dead-simple alternative to libuv for performing platform-specific operations in a non-blocking and platform-independent way.

## Core architecture

This library works more or less the same way as libuv - a thread pool is used for blocking operations, which then push their results to a message queue, while non-blocking operations return their results directly from the poll functions (without going thru the queue).

Where libyaooi differs from libuv is that it ONLY does IO, and instead of using callbacks to deliver messages, a `yo_req_t` object is used to keep track of async requests. It is up to the user code to associate said object with any useful callback/coroutine/userdata.

Another advantage over libuv we have is that libyaooi provides sync functions, which are just simple wrappers around the underlying functions (so you can use this library as just a cross-platform layer over IO and OS operations).

## Why not libuv?

libuv has grown into a beast - it has almost 100K LOC, requires three separate pieces of software to be built and completely takes over your event loop. This is simply because libuv tries to do everything - the event loop and IO operations. libyaooi specializes in only giving you an interface to a set of common OS operations + the OS's polling mechanism. If you seek a simple and robust IO library to use in your next runtime/interpreter, libyaooi is your best choice.

## Why libuv?

Although libyaooi is vastly simpler than libuv, libuv has a lot more support behind it and has been battle-tested for the better part of the past two decades.

## Backends

Available:

- Win32 (sync)
- Posix (sync)
- ansic (sync)
- epoll (async)
- poll (async)

Planned:

- /dev/poll (async)
- kqueue (async)
- Win32 IOCP (async)

NOT planned:

- io_uring (past versions had it, it was a major pain for almost no benefit)
- Messenger pigeons (tried it, there was too much bird poo involved)

## General pattern of usage

An example luajit FFI wrapper has been included, so that you can get an idea of how to use the library. Every function has been documented in the header, you can take a look at that. But at a high level, this is what you want to do (in pseudocode):

```
list<coroutine> tasks = [];
map<yo_req_t, coroutine> reqs = {};
yo_queue_t queue = yo_queue_new();

func sync_call(function yo_func, yo_t queue, ...) {
	yo_req_t req = yo_req_new(queue);
	yo_func(req, coro_running(), ...);
	reqs[req] = current_thread;
	return yield();
}

func run_loop() {
	while (true) {
		while (tasks.length > 0) {
			coroutine task = tasks.remove(0);
			resume(task);
		}

		// Exit out when all operations are complete
		if (reqs.length == 0 && tasks.length == 0) break;

		yo_req_t req;
		int err;
		if (!yo_queue_poll(queue, NULL, &req, &err)) break;

		resume(reqs[req], err);
	}
}

func main() {
	tasks.add(coroutine {
		yo_fd_t stdout;
		yo_tty_out(&stdout);
		yo_fd_t f;
		sync_call(yo_open, queue, &f, "myfile.txt", yo_OPEN_READ);

		size_t i = 0;
		while (true) {
			size_t n = 1024;
			char buff[1024];

			sync_call(yo_read, queue, f, &n, buff, i);
			if (n == 0) break;

			i += n;
			sync_call(yo_write, queue, stdout, &n, buff, i);
		}
	});

	run_loop();
}

```
