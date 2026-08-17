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
	typedef struct {
		evi_pool_worker_t worker;
	} evi_req_task_t;
	typedef struct ev_pool {
		evi_pool_worker_t worker_head;
		#define evi_list_pool_worker_next(node) (node)->next
	} evi_pool_s, *evi_pool_t;
#else
	typedef struct {
	} evi_req_task_t;
	typedef struct ev_pool {
	} evi_pool_s, *evi_pool_t;
#endif

static void evi_pool_init(evi_pool_t pool);
static void evi_pool_free(evi_pool_t pool);
