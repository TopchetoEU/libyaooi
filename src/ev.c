#pragma GCC diagnostic ignored "-Wunused-function"

// IWYU pragma: private

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>

#include <ev/conf.h>
#include <ev/errno.h>

// Source files included here for a unity build

#include "./addr.c" // IWYU pragma: export
#include "./filelist.c" // IWYU pragma: export
#include "./queue.c" // IWYU pragma: export
#include "./time.c" // IWYU pragma: export
#include "./impl.c" // IWYU pragma: export

const char *ev_strerr(ev_code_t code) {
	switch (code) {
		#define EV_SIGDEF_SWITCH_X(name, code, msg) case code: return msg;
		EV_SIGDEF(EV_SIGDEF_SWITCH_X)
		// #undef EV_SIGDEF_SWITCH_X
		default: return "unknown OS-specific error";
	}
}

// ev_code_t ev_init(ev_t ev) {
// 	ev->active_n = 0;

// 	if (evi_impl_init(ev) < 0) goto fail;
// 	if (evi_queue_init(ev) < 0) goto fail_sync;

// 	#ifdef EV_USE_MULTITHREAD
// 		evi_pool_init(ev->pool);
// 	#endif

// 	return ev;
// fail_sync:
// 	evi_impl_free(ev);
// fail:
// 	free(ev);
// 	return NULL;
// }
// void ev_free(ev_t ev) {
// 	#ifdef EV_USE_MULTITHREAD
// 		evi_pool_free(ev->pool);
// 	#endif

// 	evi_queue_free(ev);
// 	evi_impl_free(ev);
// 	evi_async_free(ev);
// }

// bool ev_busy(ev_t ev) {
// 	return ev->active_n > 0;
// }

// void ev_begin(ev_t ev) {
// 	ev->active_n++;
// }
// void ev_end(ev_t ev) {
// 	ev->active_n--;
// }
// ev_code_t ev_exec(ev_t ev, void *udata, ev_worker_t worker, void *pargs, bool sync) {
// 	(void)sync;

// 	#ifdef EV_USE_MULTITHREAD
// 		if (!sync) {
// 			return evi_pool_exec(ev, ev->pool, udata, worker, pargs);
// 		}
// 	#endif

// 	ev_begin(ev);
// 	int code = worker(pargs);
// 	if (code == EV_ECANCELED) return EV_ECANCELED;

// 	return ev_push(ev, udata, code);
// }

// ev_hnd_t ev_tty_in(ev_t ev) {
// 	return ev->in;
// }
// ev_hnd_t ev_tty_out(ev_t ev) {
// 	return ev->out;
// }
// ev_hnd_t ev_tty_err(ev_t ev) {
// 	return ev->err;
// }
