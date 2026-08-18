#pragma once
#include <yaooi/conf.h>

#ifdef /* multiplex */ YO_USE_EPOLL
	#include "./epoll.h" // IWYU pragma: export
#elif defined YO_USE_UNIX
	#include "./poll.h" // IWYU pragma: export
#endif
