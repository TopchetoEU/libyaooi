#ifndef YO_SIGNO_H
#define YO_SIGNO_H

// Signals in libev do not correlate to OS signals 1:1. You should treat these more like semantic events to be handled
typedef enum {
	// The user pressed Ctrl+C
	YO_SIGINT,
	// The user pressed 'Ctrl+\'
	YO_SIGQUIT,
	// An abort has been triggered (usually from the abort() call)
	YO_SIGABRT,
	// Your process is being killed! You may choose to ignore this
	YO_SIGTERM,

	// Fired when a memory error occurred. Ususally, your app will not live long enough to handle it.
	// Combines SIGSEGV, SIGBUS, SIGSTKFLT
	YO_SIGBADMEM,
	// A bad instruction or syscall was performed. Ususally, your app will not live long enough to handle it.
	// Combines SIGILL and SIGSYS
	YO_SIGBADOP,
	// The OS decided that it didn't like how you are doing networking, and is trying to murder you in cold blood.
	// You definitely want to ignore this one, and instead handle `errno`
	// Correlates to SIGPIPE
	YO_SIGBADPIPE,

	// Terminal has changed its size
	// Correlates to SIGWINCH
	YO_SIGTSIZE,
	// Terminal has been disconnected
	// If you're a daemon, ignore this.
	// Correlates to SIGHUP
	YO_SIGTLOST,

	// First user-defined signal was received
	YO_SIGUSR1,
	// Second user-defined signal was received
	YO_SIGUSR2,
} yo_signo_t;

#endif
