#pragma once

#include <stdbool.h>

#include <ev/conf.h>
#include <ev/errno.h>
#include <ev/queue.h>
#include <stdlib.h>

#include "./queue.h" // IWYU pragma: export

#include "./utils/multithread.h"
#include "./utils/lists.h"

#include "./impl.c"
#include "./pool.c"

static bool evi_queue_trykill(ev_queue_t queue) {
	if (!queue->dead) goto fail;
	if (queue->running) goto fail;

	while (queue->ready) {
		queue->ready->state = EVI_REQ_DEAD;
	}

	ev_mutex_unlock(queue->lock);
	ev_mutex_free(queue->lock);

	free(queue);

	return true;

fail:
	ev_mutex_unlock(queue->lock);
	return false;
}
static bool evi_req_begin(ev_req_t req, void (*cancel)(ev_req_t req)) {
	ev_mutex_lock(req->queue->lock);

	if (req->state != EVI_REQ_BORN) {
		ev_mutex_unlock(req->queue->lock);
		return false;
	}

	req->state = EVI_REQ_RUNNING;
	req->running.cancel = cancel;
	req->running.cancelled = false;
	evi_dlist_add(req_running, req->queue->running, req);

	ev_mutex_unlock(req->queue->lock);
	return true;
}
static bool evi_req_end(ev_req_t req, ev_code_t code) {
	ev_mutex_lock(req->queue->lock);

	if (req->state != EVI_REQ_RUNNING) {
		ev_mutex_unlock(req->queue->lock);
		return false;
	}

	evi_dlist_del(req_running, req);

	if (req->queue->dead) {
		req->state = EVI_REQ_DEAD;
		if (evi_queue_trykill(req->queue)) return true;
	}
	else {
		req->state = EVI_REQ_READY;
		req->ready.code = code;
		evi_list_add(req_ready, req->queue->ready, req);
	}

	evi_queue_impl_notify(req->queue);

	ev_mutex_unlock(req->queue->lock);
	return true;
}
static bool evi_req_kill(ev_req_t req) {
	ev_mutex_lock(req->queue->lock);

	if (req->state != EVI_REQ_RUNNING) {
		ev_mutex_unlock(req->queue->lock);
		return false;
	}

	evi_dlist_del(req_running, req);
	req->state = EVI_REQ_DEAD;

	if (req->queue->dead) {
		if (!evi_queue_trykill(req->queue)) {
			ev_mutex_unlock(req->queue->lock);
		}
	}

	return true;
}

static ev_req_t evi_queue_pop(ev_queue_t queue, ev_code_t *pcode) {
	ev_mutex_lock(queue->lock);

	ev_req_t req = queue->ready;
	if (!req) {
		ev_mutex_unlock(queue->lock);
		return NULL;
	}

	req->state = EVI_REQ_DEAD;
	evi_list_del(req_ready, queue->ready);

	if (!evi_queue_trykill(queue)) {
		ev_mutex_unlock(queue->lock);
	}

	*pcode = req->ready.code;
	return req;
}

ev_queue_t ev_queue_new() {
	ev_queue_t queue = malloc(sizeof *queue);
	if (!queue) return NULL;

	ev_code_t err = evi_queue_impl_init(queue);
	if (err != EV_OK) return NULL;

	queue->ready = NULL;
	queue->running = NULL;
	queue->dead = false;

	evi_pool_init(&queue->pool);
	ev_mutex_new(queue->lock);

	return queue;
}
void ev_queue_free(ev_queue_t queue) {
	ev_mutex_lock(queue->lock);

	evi_pool_free(&queue->pool);

	if (queue->dead) {
		ev_mutex_unlock(queue->lock);
		return;
	}

	for (ev_req_t i = queue->running; i; i = i->running.next) {
		ev_req_cancel(i);
	}

	ev_code_t err = evi_queue_impl_free(queue);
	if (err != EV_OK) {
		ev_mutex_unlock(queue->lock);
		return;
	}

	evi_queue_trykill(queue);
}

void ev_req_cancel(ev_req_t req) {
	ev_mutex_lock(req->queue->lock);

	if (req->state != EVI_REQ_RUNNING) {
		ev_mutex_unlock(req->queue->lock);
		return;
	}
	if (req->running.cancelled) {
		ev_mutex_unlock(req->queue->lock);
		return;
	}

	req->running.cancel(req);
	req->running.cancelled = true;

	ev_mutex_unlock(req->queue->lock);
}
void ev_req_free(ev_req_t req) {
	assert(req->state != EVI_REQ_DEAD);
	free(req);
}
ev_code_t ev_req_exec(ev_req_t req, ev_worker_t worker, void *args) {
	return evi_pool_exec(&req->queue->pool, req, worker, args);
}
