#ifndef EV_CONF_H
#define EV_CONF_H

// 1. Detect target

#ifdef __linux
	#define EV_USE_UNIX
	#define EV_USE_LINUX
#elif defined __unix__
	#define EV_USE_UNIX
#elif defined WIN32
	#define EV_USE_WIN32
#endif

// 2. Apply user overrides for the system

#ifdef EV_NO_USE_WIN32
	#undef EV_USE_WIN32
#endif
#ifdef EV_NO_USE_UNIX
	#undef EV_USE_UNIX
	#undef EV_USE_LINUX
#endif
#ifdef EV_NO_USE_LINUX
	#undef EV_USE_LINUX
#endif

// 3. Infer sensible defaults for features from target

#ifdef EV_USE_LINUX
	#define EV_USE_MULTITHREAD
	#define EV_USE_EPOLL
#elif defined EV_USE_UNIX
	#define EV_USE_MULTITHREAD
	#define EV_USE_POLL
#elif defined EV_USE_WIN32
	#define EV_USE_MULTITHREAD
#endif

// 4. Apply user blacklists for features

#ifdef EV_NO_USE_EPOLL
	#undef EV_USE_EPOLL
#endif
#ifdef EV_NO_USE_POLL
	#undef EV_USE_POLL
#endif
#ifdef EV_NO_USE_MULTITHREAD
	#undef EV_USE_MULTITHREAD
#endif

#endif
