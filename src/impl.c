#pragma once
#include <ev/conf.h>

#ifdef /* multiplex */ EV_USE_UNIX
	#include "./unix/impl.c" // IWYU pragma: export
#elif defined EV_USE_WIN32
	#include "./win/impl.c" // IWYU pragma: export
#else
	#include "./ansi/impl.c" // IWYU pragma: export
#endif
