#pragma once

#include <stdbool.h>

#include <ev/queue.h>

#include "./utils/multithread.h"

#ifdef EV_USE_MULTITHREAD
	typedef struct ev_pool_worker {
		struct ev_pool_worker *next;
		ev_queue_t queue;
		ev_mutex_t lock;
		ev_cond_t cond;

		ev_thread_t thread;

		ev_worker_t worker;
		ev_req_t req;
		void *args;

		bool kys;
	} *evi_pool_worker_t;
#endif

typedef struct ev_pool {
	#ifdef EV_USE_MULTITHREAD
		evi_pool_worker_t worker_head;
		#define evi_list_pool_worker_next(node) (node)->next
	#endif
} eiv_pool_s, *evi_pool_t;

typedef struct {
	#ifdef EV_USE_MULTITHREAD
		evi_pool_worker_t worker;
	#endif
} evi_req_task_t;

static ev_code_t evi_pool_exec(evi_pool_t pool, ev_req_t req, ev_worker_t worker, void *args);
static void evi_pool_init(evi_pool_t pool);
static void evi_pool_free(evi_pool_t pool);
