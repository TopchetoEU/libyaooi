// libyaooi, Copyright (C) 2025-2026 topchetoeu, see LICENSE for full LGPL text

#pragma once
#include <yaooi/conf.h>

#ifdef /* multiplex */ YO_USE_UNIX
	#include "./unix/impl.c" // IWYU pragma: export
#elif defined YO_USE_WIN32
	#include "./win/impl.c" // IWYU pragma: export
#else
	#include "./ansi/impl.c" // IWYU pragma: export
#endif
