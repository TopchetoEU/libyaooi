#pragma once

#include <yaooi/conf.h>

#include "./async.h" // IWYU pragma: export

// IWYU pragma: begin_exports
#ifdef /* multiplex */ YO_USE_EPOLL
	#include "./epoll.c"
#elif defined YO_USE_UNIX
	#include "./poll.c"
#endif
// IWYU pragma: end_exports
