// libyaooi, Copyright (C) 2025-2026 topchetoeu, see LICENSE for full LGPL text

#pragma once

#include <stdint.h>
#include <signal.h> // IWYU pragma: keep
#include <errno.h> // IWYU pragma: keep
#include <stdlib.h>

#include <yaooi/time.h>
#include <yaooi/conf.h>
#include <yaooi/errno.h>

#if defined YO_USE_MULTITHREAD && defined YO_USE_WIN32
	#include <winsock2.h>
	#include <windows.h>

	typedef HANDLE yo_thread_t[1];
	typedef CRITICAL_SECTION yo_mutex_t[1];
	typedef CONDITION_VARIABLE yo_cond_t[1];

	#define yo_thread_new(th, entry, args) (CreateThread(NULL, 0, (void*)(entry), (args), 0, NULL) ? 0 : -1)
	#define yo_thread_free_join(th) (WaitForSingleObject(th, INFINITE) ? 0 : -1)
	#define yo_thread_cancel(th) (CancelSynchronousIo(th) ? 0 : -1)

	#define yo_mutex_new(mut) (void)InitializeCriticalSection(mut)
	#define yo_mutex_free(mut) (void)DeleteCriticalSection(mut)
	#define yo_mutex_lock(mut) (void)EnterCriticalSection(mut)
	#define yo_mutex_unlock(mut) (void)LeaveCriticalSection(mut)

	#define yo_cond_new(cond) (void)InitializeConditionVariable(cond)
	// #define yo_cond_free(cond) (DeleteConditionVariable(cond), 0)
	#define yo_cond_free(cond) ((void)cond)
	#define yo_cond_wait(cond, mut) (void)SleepConditionVariableCS(cond, mut, INFINITE)
	static inline yo_code_t yo_cond_timewait(yo_cond_t cond, yo_mutex_t mut, yo_time_t timeout) {
		int64_t ms = yo_timems(yo_timesub(timeout, yo_time(YO_CLOCK_MONO)));
		if (ms < 0) ms = 0;

		if (!SleepConditionVariableCS(cond, mut, ms)) return YO_ETIMEDOUT;
		return 0;
	}
	#define yo_cond_broadcast(cond) (void)WakeAllConditionVariable(cond)
	#define yo_cond_signal(cond) (void)WakeConditionVariable(cond)
#elif defined YO_USE_MULTITHREAD && defined YO_USE_UNIX
	#include <pthread.h>

	typedef pthread_t yo_thread_t[1];
	typedef pthread_mutex_t yo_mutex_t[1];
	typedef pthread_cond_t yo_cond_t[1];

	typedef struct {
		void (*entry)(void *pargs);
		void *pargs;
	} *yo_thread_args_t;

	static void yo_thread_sighandle(int sig) {
		(void)sig;
	}
	static void *yo_thread_entry(void *pargs) {
		yo_thread_args_t args = pargs;
		void (*entry)(void *pargs) = args->entry;
		void *entry_pargs = args->pargs;
		free(args);

		sigset_t set;
		sigfillset(&set);
		sigdelset(&set, SIGPWR);
		pthread_sigmask(SIG_SETMASK, &set, NULL);

		struct sigaction sig_act = {
			.sa_handler = yo_thread_sighandle,
			.sa_flags = 0,
		};
		sigemptyset(&sig_act.sa_mask);
		sigaction(SIGPWR, &sig_act, NULL);

		entry(entry_pargs);
		return NULL;
	}

	static inline int yo_thread_new(yo_thread_t th, void (*entry)(void *pargs), void *pargs) {
		yo_thread_args_t args = malloc(sizeof *args);
		args->entry = entry;
		args->pargs = pargs;

		return pthread_create(th, (const pthread_attr_t*)NULL, yo_thread_entry, args);
	}
	#define yo_thread_cancel(th) (void)pthread_kill(*(th), SIGPWR)

	static inline void *yo_thread_free_join(yo_thread_t th) {
		void *ret;
		pthread_join(*th, &ret);
		return ret;
	}

	#define yo_mutex_new(mut) (void)pthread_mutex_init(mut, (const pthread_mutexattr_t *)NULL)
	#define yo_mutex_free(mut) (void)pthread_mutex_destroy(mut)
	#define yo_mutex_lock(mut) (void)pthread_mutex_lock(mut)
	#define yo_mutex_unlock(mut) (void)pthread_mutex_unlock(mut)
	#define yo_setmask pthread_sigmask

	static inline void yo_cond_new(yo_cond_t cond) {
		pthread_condattr_t attr[1];
		pthread_condattr_init(attr);
		pthread_condattr_setclock(attr, CLOCK_MONOTONIC);
		pthread_cond_init(cond, attr);
		pthread_condattr_destroy(attr);
	}
	#define yo_cond_free(cond) pthread_cond_destroy(cond)
	#define yo_cond_signal(cond) (void)pthread_cond_signal(cond)
	#define yo_cond_broadcast(cond) (void)pthread_cond_signal(cond)
	#define yo_cond_wait(cond, mut) (void)pthread_cond_wait(cond, mut)
	static inline yo_code_t yo_cond_timewait(yo_cond_t cond, yo_mutex_t mut, yo_time_t timeout) {
		int code = pthread_cond_timedwait(cond, mut, &(struct timespec) { .tv_sec = timeout.sec, .tv_nsec = timeout.nsec });
		if (code == ETIMEDOUT) return YO_ETIMEDOUT;
		return 0;
	}
#elif defined YO_USE_MULTITHREAD
	#error Multithreading enabled on non-multithreaded platform
#else
	typedef struct {} yo_mutex_t[1];

	#define yo_mutex_new(mut)
	#define yo_mutex_free(mut)
	#define yo_mutex_lock(mut)
	#define yo_mutex_unlock(mut)

	#if defined YO_USE_UNIX
		#define yo_setmask sigprocmask
	#endif
#endif
