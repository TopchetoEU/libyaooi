// libyaooi, Copyright (C) 2025-2026 topchetoeu, see LICENSE for full LGPL text

#pragma once

#include <stdbool.h>

#include <yaooi/errno.h>
#include <yaooi/queue.h>

#include "./pool.h" // IWYU pragma: export

#include "./utils/multithread.h"
#include "./queue.h"

#include "./queue.c"

#ifdef YO_USE_MULTITHREAD
	static void _yoi_pool_cancelcb(yo_req_t req) {
		yo_thread_cancel(req->running.task.worker->thread);
	}

	static void yoi_pool_worker_entry(void *pargs) {
		yoi_pool_worker_t worker = (yoi_pool_worker_t)pargs;

		yo_mutex_lock(worker->lock);

		while (true) {
			while (worker->worker && !worker->kys) {
				yo_req_t req = worker->req;
				yo_worker_t cb = worker->worker;
				void *args = worker->args;

				yo_mutex_unlock(worker->lock);

				yoi_req_end(req, cb(args));

				yo_mutex_lock(worker->lock);
				worker->worker = NULL;
				worker->args = NULL;
				worker->req = NULL;
			}
			if (worker->kys) break;

			yo_cond_wait(worker->cond, worker->lock);
		}

		yo_mutex_unlock(worker->lock);
	}

	yo_code_t yo_req_exec(yo_req_t req, yo_worker_t worker, void *args) {
		yoi_pool_t pool = &req->queue->pool;
		for (yoi_pool_worker_t it = pool->worker_head; it; it = it->next) {
			yo_mutex_lock(it->lock);
			if (!it->worker && !it->kys) {
				it->req = req;
				it->worker = worker;
				it->args = args;
				yo_cond_broadcast(it->cond);
				yo_mutex_unlock(it->lock);

				goto begin;
			}
			yo_mutex_unlock(it->lock);
		}

		yoi_pool_worker_t pool_worker = malloc(sizeof *pool_worker);
		if (!pool_worker) return YO_ENOMEM;

		yo_cond_new(pool_worker->cond);
		yo_mutex_new(pool_worker->lock);

		pool_worker->req = req;
		pool_worker->kys = false;

		pool_worker->worker = worker;
		pool_worker->args = args;
		pool_worker->next = NULL;

		if (yo_thread_new(pool_worker->thread, yoi_pool_worker_entry, pool_worker) < 0) {
			yo_cond_free(pool_worker->cond);
			free(pool_worker);
			return YO_EAGAIN;
		}

		yoi_list_add(pool_worker, pool->worker_head, pool_worker);

	begin:
		yoi_req_begin(req, _yoi_pool_cancelcb);
		return YO_OK;
	}

	static void yoi_pool_init(yoi_pool_t pool) {
		pool->worker_head = NULL;
	}
	static void yoi_pool_free(yoi_pool_t pool) {
		while (pool->worker_head) {
			yoi_pool_worker_t curr = pool->worker_head;
			pool->worker_head = curr->next;

			yo_mutex_lock(curr->lock);
			curr->kys = true;
			yo_thread_cancel(curr->thread);
			yo_cond_broadcast(curr->cond);
			yo_mutex_unlock(curr->lock);

			yo_thread_free_join(curr->thread);

			yo_cond_free(curr->cond);
			yo_mutex_free(curr->lock);
			free(curr);
		}

		pool->worker_head = NULL;
	}
#else
	static yo_code_t yoi_pool_exec(yoi_pool_t pool, yo_req_t req, yo_worker_t worker, void *args) {
		(void)pool;
		yoi_req_begin(req, yoi_req_cancel_noop_cb);
		yoi_req_end(req, worker(args));
		return YO_OK;
	}
	static void yoi_pool_init(yoi_pool_t pool) {
		(void)pool;
	}
	static void yoi_pool_free(yoi_pool_t pool) {
		(void)pool;
	}
#endif
