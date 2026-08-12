#pragma once

#include <stdbool.h>

#include <ev/conf.h>
#include <ev/errno.h>
#include <ev/queue.h>

#include "../utils/multithread.h"
#include "../impl/impl.h"
#include "./pool.h"

typedef struct ev_queue {
	ev_mutex_t lock;

	ev_req_t running;
	ev_req_t ready;

	bool dead;

	struct evi_queue_impl impl;
} *ev_queue_t;

static ev_req_t evi_queue_pop(ev_queue_t queue, ev_code_t *pcode);
static ev_code_t evi_queue_init(ev_queue_t ev);
static ev_code_t evi_queue_free(ev_queue_t ev);

typedef enum {
	EVI_REQ_BORN,
	EVI_REQ_RUNNING,
	EVI_REQ_READY,
	EVI_REQ_DEAD,
} evi_req_state_t;

struct ev_req {
	ev_queue_t queue;
	evi_req_state_t state;

	union {
		struct {
			ev_req_t *slot;
			ev_req_t next;
			#define evi_list_req_running_next(node) (node)->running.next
			#define evi_list_req_running_slot(node) (node)->running.slot

			void (*cancel)(ev_req_t req);
			bool cancelled;

			union {
				// struct evi_req_ioq ioq;
				struct evi_req_task task;
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
