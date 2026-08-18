#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include <ev/conf.h>
#include <ev/errno.h>
#include <ev/queue.h>

#include "./queue.h" // IWYU pragma: export

#include "./utils/multithread.h"
#include "./utils/lists.h"

#include "./impl.c"
#include "./pool.c"

static void evi_req_begin(ev_req_t req, void (*cancel)(ev_req_t req)) {
	req->running.cancel = cancel;
	req->running.cancelled = false;
}
static void evi_req_end(ev_req_t req, ev_code_t code) {
	req->ready.code = code;

	// TODO: do with CAS
	ev_mutex_lock(req->queue->lock);
	evi_list_add(req_ready, req->queue->head, req);
	evi_queue_impl_notify(req->queue);
	ev_mutex_unlock(req->queue->lock);
}

static void evi_req_cancel_noop_cb(ev_req_t req) {
	evi_req_end(req, EV_ECANCELED);
}

static ev_req_t evi_queue_pop(ev_queue_t queue, ev_code_t *pcode) {
	ev_mutex_lock(queue->lock);

	ev_req_t req = queue->head;
	if (!req) {
		ev_mutex_unlock(queue->lock);
		return NULL;
	}

	evi_list_del(req_ready, queue->head);

	ev_mutex_unlock(queue->lock);

	*pcode = req->ready.code;
	return req;
}

ev_queue_t ev_queue_new() {
	ev_queue_t queue = malloc(sizeof *queue);
	if (!queue) return NULL;

	ev_code_t err = evi_queue_impl_init(queue);
	if (err != EV_OK) return NULL;

	queue->head = NULL;

	evi_pool_init(&queue->pool);
	ev_mutex_new(queue->lock);

	return queue;
}
void ev_queue_free(ev_queue_t queue) {
	ev_mutex_lock(queue->lock);

	evi_pool_free(&queue->pool);

	ev_code_t err = evi_queue_impl_free(queue);
	if (err != EV_OK) {
		ev_mutex_unlock(queue->lock);
		return;
	}

	ev_mutex_free(queue->lock);

	free(queue);
}

ev_req_t ev_req_new(ev_queue_t queue) {
	ev_req_t res = malloc(sizeof *res);
	if (!res) return NULL;

	res->queue = queue;
	return res;
}
void ev_req_cancel(ev_req_t req) {
	if (req->running.cancelled) return;

	req->running.cancel(req);
	req->running.cancelled = true;
}
void ev_req_free(ev_req_t req) {
	free(req);
}
