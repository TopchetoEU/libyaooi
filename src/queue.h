// libyaooi, Copyright (C) 2025-2026 topchetoeu, see LICENSE for full LGPL text

#pragma once

#include <stdbool.h>

#include <yaooi/conf.h>
#include <yaooi/errno.h>
#include <yaooi/queue.h>

#include "./utils/multithread.h"
#include "./impl.h"
#include "./pool.h"

struct yo_queue {
	yo_mutex_t lock;
	yo_req_t head;
	yoi_pool_s pool;
	yoi_queue_impl_t impl;
};

static yo_req_t yoi_queue_pop(yo_queue_t queue, yo_code_t *pcode);

struct yo_req {
	yo_queue_t queue;

	union {
		struct {
			// TODO: should a FD keep a list of *all* requests. For now, only IOQs are kept
			// yo_req_t fd_next, *fd_slot;
			// #define yoi_list_req_fd_next(node) (node)->running.next
			// #define yoi_list_req_fd_slot(node) (node)->running.slot

			void (*cancel)(yo_req_t req);
			bool cancelled;

			union {
				yoi_req_ioq_t ioq;
				yoi_req_task_t task;
			};
		} running;
		struct {
			yo_code_t code;
			yo_req_t next;
			#define yoi_list_req_ready_next(node) (node)->ready.next
		} ready;
	};

	// yo_req_t queue_next;void
	// void *udata;
	// struct yoi_req_impl impl;
};

// Puts the request on its queue. `req->ready` is invalidated
static void yoi_req_begin(yo_req_t req, void (*cancel)(yo_req_t req));
// Puts the request on its queue. `req->running` is invalidated
static void yoi_req_end(yo_req_t req, yo_code_t code);

// Can be passed as a NOOP callback for cancellation
static void yoi_req_cancel_noop_cb(yo_req_t req);
