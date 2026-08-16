#pragma once

#include <ev/conf.h>

#include "./async.h" // IWYU pragma: export

// IWYU pragma: begin_exports
#ifdef /* multiplex */ EV_USE_URING
	#include "./uring.c"
#elif defined EV_USE_EPOLL
	#include "./epoll.c"
#elif defined EV_USE_POSIX
	#include "./poll.c"
#endif
// IWYU pragma: end_exports
