#pragma once
#include <ev/conf.h>

#ifdef EV_USE_POSIX
	#include "./unix/impl.h" // IWYU pragma: export
#elif defined EV_USE_WIN32
	#include "./win/impl.h" // IWYU pragma: export
#else
	#include "./ansi/impl.h" // IWYU pragma: export
#endif
