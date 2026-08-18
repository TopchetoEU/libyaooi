#pragma once

#include <yaooi/queue.h>
#include <yaooi/errno.h>
#include <yaooi/time.h>

#include "../utils/multithread.h"

#include "./queue.h" // IWYU pragma: export

#include "../queue.c"

void yoi_queue_impl_notify(yo_queue_t queue) {
	#ifdef YO_USE_MULTITHREAD
		yo_cond_broadcast(queue->impl.cond);
	#else
		(void)queue;
	#endif
}

#ifndef yo_queue_poll
	yo_code_t (yo_queue_poll)(yo_queue_t queue, const yo_time_t *pdeadline, yo_req_t *preq, yo_code_t *pcode) {
		while (true) {
			yo_req_t req = yoi_queue_pop(queue, pcode);
			if (req) {
				*preq = req;
				return YO_OK;
			}

			#ifdef YO_USE_MULTITHREAD
				yo_mutex_lock(queue->lock);
				if (pdeadline) yo_cond_timewait(queue->impl.cond, queue->lock, *pdeadline);
				else yo_cond_wait(queue->impl.cond, queue->lock);
				yo_mutex_unlock(queue->lock);
			#else
				if (pdeadline && yo_timecmp(yo_time(YO_CLOCK_MONOTIME), *pdeadline) > 0) return YO_ETIMEDOUT;
			#endif
		}
	}
#endif


yo_code_t yoi_queue_impl_init(yo_queue_t queue) {
	#ifdef YO_USE_MULTITHREAD
		yo_cond_new(queue->impl.cond);
	#else
		(void)queue;
	#endif

	return YO_OK;
}
yo_code_t yoi_queue_impl_free(yo_queue_t queue) {
	#ifdef YO_USE_MULTITHREAD
		yo_cond_free(queue->impl.cond);
	#else
		(void)queue;
	#endif

	return YO_OK;
}
