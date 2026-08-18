**THIS IS A DEV BRANCH, EXCPECT STUFF TO BREAK!!!**

libyaooi (Yet Another OS Operations Interface, you pervert) is a dead-simple alternative to libuv for performing platform-specific operations in a non-blocking and platform-independent way.

## Core architecture

This library works more or less the same way as libuv - a thread pool is used for blocking operations, which then push their results to a message queue, while non-blocking operations push their results on the queue when the non-blocking operation's callback is called.

Where libyaooi differs from libuv is that it ONLY does IO, and instead of using callbacks to deliver messages, a `void*` is passed to the IO function, which then is returned from the message polling function, alongside an error code. It is up to the user code to determine what the semantic meaning of this user data is.

Of course, you can (and are encouraged to) implement some sort of callback system on top of this system (in my lua wrapper, this is done with a simple table of incremental udata -> callback).

## Why not libuv?

libuv has a notoriously difficult build process - in comparison, libyaooi is a unity build - you can build it with a single gcc command. Also, the callback nature of libuv makes it a PITA to use in managed languages. libuv also, for some reason, decides to implement an utterly baffling OOP inheritance chain of different handles. libyaooi does none of that and is mostly procedural. Last but not least, libuv is a whopping 70K lines of code, while libyaooi doesn't even clock in at 2K LOC, and yet does more or less the same things libuv does (except for event queue management, which is delegated to client code, but a quality implementation should fit in under 5K LOC).

## Why libuv?

Make no mistake, libyaooi is a hobby project and is largely untested, while libuv has been battle-tested for more than 10 years, so you can most likely count on it. Also, libyaooi still doesn't offer support for some of the stuff libuv offers (but it is trivially simple to implement them, as libyaooi exposes a `yo_exec` function, which executes a function in the threadpool of libyaooi and returns the result in the message queue).

## Supported async backends

- epoll
- poll
- /dev/poll (planned)
- kqueue (planned)
- whatever windows offers (planned)

(uring is NOT planned, as it is bullshit)

## Supported systems

- ANSI (very rudimentary, whatever ANSIC has)
- Posix
- Win32

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
