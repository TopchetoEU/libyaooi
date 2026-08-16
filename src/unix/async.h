#pragma once
#include <ev/conf.h>

#ifdef /* multiplex */ EV_USE_URING
	#include "./uring.h" // IWYU pragma: export
#elif defined EV_USE_EPOLL
	#include "./epoll.h" // IWYU pragma: export
#elif defined EV_USE_POSIX
	#include "./poll.h" // IWYU pragma: export
#endif
