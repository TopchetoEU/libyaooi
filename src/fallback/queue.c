#pragma once

#include <ev/queue.h>
#include <ev/errno.h>
#include <ev/time.h>

#include "../utils/multithread.h"

#include "./queue.h" // IWYU pragma: export

#include "../queue.c"

void evi_queue_impl_notify(ev_queue_t queue) {
	#ifdef EV_USE_MULTITHREAD
		ev_cond_broadcast(queue->impl.cond);
	#else
		(void)queue;
	#endif
}

#ifndef ev_queue_poll
	ev_code_t (ev_queue_poll)(ev_queue_t queue, const ev_time_t *pdeadline, ev_req_t *preq, ev_code_t *pcode) {
		while (true) {
			ev_req_t req = evi_queue_pop(queue, pcode);
			if (req) {
				*preq = req;
				return EV_OK;
			}

			#ifdef EV_USE_MULTITHREAD
				ev_mutex_lock(queue->lock);
				if (pdeadline) ev_cond_timewait(queue->impl.cond, queue->lock, *pdeadline);
				else ev_cond_wait(queue->impl.cond, queue->lock);
				ev_mutex_unlock(queue->lock);
			#else
				if (pdeadline && ev_timecmp(ev_time(EV_CLOCK_MONOTIME), *pdeadline) > 0) return EV_ETIMEDOUT;
			#endif
		}
	}
#endif


ev_code_t evi_queue_impl_init(ev_queue_t queue) {
	#ifdef EV_USE_MULTITHREAD
		ev_cond_new(queue->impl.cond);
	#else
		(void)queue;
	#endif

	return EV_OK;
}
ev_code_t evi_queue_impl_free(ev_queue_t queue) {
	#ifdef EV_USE_MULTITHREAD
		ev_cond_free(queue->impl.cond);
	#else
		(void)queue;
	#endif

	return EV_OK;
}
