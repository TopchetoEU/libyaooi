#pragma once

#include <stdbool.h>

#include <ev/conf.h>
#include <ev/errno.h>
#include <ev/queue.h>

#include "./utils/multithread.h"
#include "./impl.h"
#include "./pool.h"

struct ev_queue {
	ev_mutex_t lock;
	ev_req_t head;
	evi_pool_s pool;
	evi_queue_impl_t impl;
};

static ev_req_t evi_queue_pop(ev_queue_t queue, ev_code_t *pcode);

struct ev_req {
	ev_queue_t queue;

	union {
		struct {
			// TODO: should a FD keep a list of *all* requests. For now, only IOQs are kept
			// ev_req_t fd_next, *fd_slot;
			// #define evi_list_req_fd_next(node) (node)->running.next
			// #define evi_list_req_fd_slot(node) (node)->running.slot

			void (*cancel)(ev_req_t req);
			bool cancelled;

			union {
				evi_req_ioq_t ioq;
				evi_req_task_t task;
			};
		} running;
		struct {
			ev_code_t code;
			ev_req_t next;
			#define evi_list_req_ready_next(node) (node)->ready.next
		} ready;
	};

	// ev_req_t queue_next;void
	// void *udata;
	// struct evi_req_impl impl;
};

// Puts the request on its queue. `req->ready` is invalidated
static void evi_req_begin(ev_req_t req, void (*cancel)(ev_req_t req));
// Puts the request on its queue. `req->running` is invalidated
static void evi_req_end(ev_req_t req, ev_code_t code);

// Can be passed as a NOOP callback for cancellation
static void evi_req_cancel_noop_cb(ev_req_t req);
