// libyaooi, Copyright (C) 2025-2026 topchetoeu, see LICENSE for full LGPL text

#ifndef YO_CONF_H
#define YO_CONF_H

// 1. Detect target

#ifdef __linux
	#define YO_USE_UNIX
	#define YO_USE_LINUX
#elif defined __unix__
	#define YO_USE_UNIX
#elif defined WIN32
	#define YO_USE_WIN32
#endif

// 2. Apply user overrides for the system

#ifdef YO_NO_USE_WIN32
	#undef YO_USE_WIN32
#endif
#ifdef YO_NO_USE_UNIX
	#undef YO_USE_UNIX
	#undef YO_USE_LINUX
#endif
#ifdef YO_NO_USE_LINUX
	#undef YO_USE_LINUX
#endif

// 3. Infer sensible defaults for features from target

#ifdef YO_USE_LINUX
	#define YO_USE_MULTITHREAD
	#define YO_USE_EPOLL
#elif defined YO_USE_UNIX
	#define YO_USE_MULTITHREAD
	#define YO_USE_POLL
#elif defined YO_USE_WIN32
	#define YO_USE_MULTITHREAD
#endif

// 4. Apply user blacklists for features

#ifdef YO_NO_USE_EPOLL
	#undef YO_USE_EPOLL
#endif
#ifdef YO_NO_USE_POLL
	#undef YO_USE_POLL
#endif
#ifdef YO_NO_USE_MULTITHREAD
	#undef YO_USE_MULTITHREAD
#endif

#endif
