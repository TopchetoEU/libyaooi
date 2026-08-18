#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include <yaioi/conf.h>
#include <yaioi/errno.h>
#include <yaioi/queue.h>

#include "./queue.h" // IWYU pragma: export

#include "./utils/multithread.h"
#include "./utils/lists.h"

#include "./impl.c"
#include "./pool.c"

static void yoi_req_begin(yo_req_t req, void (*cancel)(yo_req_t req)) {
	req->running.cancel = cancel;
	req->running.cancelled = false;
}
static void yoi_req_end(yo_req_t req, yo_code_t code) {
	req->ready.code = code;

	// TODO: do with CAS
	yo_mutex_lock(req->queue->lock);
	yoi_list_add(req_ready, req->queue->head, req);
	yoi_queue_impl_notify(req->queue);
	yo_mutex_unlock(req->queue->lock);
}

static void yoi_req_cancel_noop_cb(yo_req_t req) {
	yoi_req_end(req, YO_ECANCELED);
}

static yo_req_t yoi_queue_pop(yo_queue_t queue, yo_code_t *pcode) {
	yo_mutex_lock(queue->lock);

	yo_req_t req = queue->head;
	if (!req) {
		yo_mutex_unlock(queue->lock);
		return NULL;
	}

	yoi_list_del(req_ready, queue->head);

	yo_mutex_unlock(queue->lock);

	*pcode = req->ready.code;
	return req;
}

yo_queue_t yo_queue_new() {
	yo_queue_t queue = malloc(sizeof *queue);
	if (!queue) return NULL;

	yo_code_t err = yoi_queue_impl_init(queue);
	if (err != YO_OK) return NULL;

	queue->head = NULL;

	yoi_pool_init(&queue->pool);
	yo_mutex_new(queue->lock);

	return queue;
}
void yo_queue_free(yo_queue_t queue) {
	yo_mutex_lock(queue->lock);

	yoi_pool_free(&queue->pool);

	yo_code_t err = yoi_queue_impl_free(queue);
	if (err != YO_OK) {
		yo_mutex_unlock(queue->lock);
		return;
	}

	yo_mutex_free(queue->lock);

	free(queue);
}

yo_req_t yo_req_new(yo_queue_t queue) {
	yo_req_t res = malloc(sizeof *res);
	if (!res) return NULL;

	res->queue = queue;
	return res;
}
void yo_req_cancel(yo_req_t req) {
	if (req->running.cancelled) return;

	req->running.cancel(req);
	req->running.cancelled = true;
}
void yo_req_free(yo_req_t req) {
	free(req);
}
