#pragma once

#include <stdbool.h>

#include <yaioi/queue.h>

#include "./utils/multithread.h"

#ifdef YO_USE_MULTITHREAD
	typedef struct yo_pool_worker {
		struct yo_pool_worker *next;
		yo_queue_t queue;
		yo_mutex_t lock;
		yo_cond_t cond;

		yo_thread_t thread;

		yo_worker_t worker;
		yo_req_t req;
		void *args;

		bool kys;
	} *yoi_pool_worker_t;
	typedef struct {
		yoi_pool_worker_t worker;
	} yoi_req_task_t;
	typedef struct yo_pool {
		yoi_pool_worker_t worker_head;
		#define yoi_list_pool_worker_next(node) (node)->next
	} yoi_pool_s, *yoi_pool_t;
#else
	typedef struct {
	} yoi_req_task_t;
	typedef struct yo_pool {
	} yoi_pool_s, *yoi_pool_t;
#endif

static void yoi_pool_init(yoi_pool_t pool);
static void yoi_pool_free(yoi_pool_t pool);
