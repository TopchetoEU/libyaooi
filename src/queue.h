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

	ev_req_t running;
	ev_req_t ready;

	bool dead;

	evi_queue_impl_t impl;
	evi_pool_s pool;
};

static ev_req_t evi_queue_pop(ev_queue_t queue, ev_code_t *pcode);

typedef enum {
	EVI_REQ_BORN,
	EVI_REQ_RUNNING,
	EVI_REQ_READY,
	EVI_REQ_DEAD,
} evi_req_state_t;

struct ev_req {
	ev_queue_t queue;
	ev_req_t queue_next;
	evi_req_state_t state;

	union {
		struct {
			ev_req_t next, *slot;
			#define evi_list_req_queue_next(node) (node)->running.next
			#define evi_list_req_queue_slot(node) (node)->running.slot

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

// Puts the request in a `running` state
// req->state must be EVI_REQ_BORN
static bool evi_req_begin(ev_req_t req, void (*cancel)(ev_req_t req));
// Puts the request in a `ready` state on its queue
// req->state must be EVI_REQ_READY
static bool evi_req_end(ev_req_t req, ev_code_t code);
// Directly kills a `running` request. DOES NOT WORK ON `ready` TASKS
static bool evi_req_kill(ev_req_t req);

// Can be passed as a NOOP callback for cancellation
static void evi_req_cancel_noop_cb(ev_req_t req);
