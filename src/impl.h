#pragma once
#include <yaioi/conf.h>

#ifdef /* multiplex */ YO_USE_UNIX
	#include "./unix/impl.h" // IWYU pragma: export
#elif defined YO_USE_WIN32
	#include "./win/impl.h" // IWYU pragma: export
#else
	#include "./ansi/impl.h" // IWYU pragma: export
#endif
