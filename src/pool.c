#pragma once

#include <stdbool.h>

#include <ev/errno.h>
#include <ev/queue.h>

#include "./pool.h" // IWYU pragma: export

#include "./utils/multithread.h"
#include "./queue.h"

#include "./queue.c"

static void _evi_pool_cancelcb(ev_req_t req) {
	#ifdef EV_USE_MULTITHREAD
		ev_thread_cancel(req->running.task.worker->thread);
	#else
		(void)req;
	#endif
}
#ifdef EV_USE_MULTITHREAD

	static void evi_pool_worker_entry(void *pargs) {
		evi_pool_worker_t worker = (evi_pool_worker_t)pargs;

		ev_mutex_lock(worker->lock);

		while (true) {
			while (worker->worker && !worker->kys) {
				ev_req_t req = worker->req;
				ev_worker_t cb = worker->worker;
				void *args = worker->args;

				ev_mutex_unlock(worker->lock);

				evi_req_end(req, cb(args));

				ev_mutex_lock(worker->lock);
				worker->worker = NULL;
				worker->args = NULL;
				worker->req = NULL;
			}
			if (worker->kys) break;

			ev_cond_wait(worker->cond, worker->lock);
		}

		ev_mutex_unlock(worker->lock);
	}

	static ev_code_t evi_pool_exec(evi_pool_t pool, ev_req_t req, ev_worker_t worker, void *args) {
		for (evi_pool_worker_t it = pool->worker_head; it; it = it->next) {
			ev_mutex_lock(it->lock);
			if (!it->worker && !it->kys) {
				it->req = req;
				it->worker = worker;
				it->args = args;
				ev_cond_broadcast(it->cond);
				ev_mutex_unlock(it->lock);

				goto begin;
			}
			ev_mutex_unlock(it->lock);
		}

		evi_pool_worker_t pool_worker = malloc(sizeof *pool_worker);
		if (!pool_worker) return EV_ENOMEM;

		ev_cond_new(pool_worker->cond);
		ev_mutex_new(pool_worker->lock);

		pool_worker->req = req;
		pool_worker->kys = false;

		pool_worker->worker = worker;
		pool_worker->args = args;
		pool_worker->next = NULL;

		if (ev_thread_new(pool_worker->thread, evi_pool_worker_entry, pool_worker) < 0) {
			ev_cond_free(pool_worker->cond);
			free(pool_worker);
			return EV_EAGAIN;
		}

		evi_list_add(pool_worker, pool->worker_head, pool_worker);

	begin:
		evi_req_begin(req, evi_pool_exec_cancel);
		return EV_OK;
	}

	static void evi_pool_init(evi_pool_t pool) {
		pool->worker_head = NULL;
	}
	static void evi_pool_free(evi_pool_t pool) {
		while (pool->worker_head) {
			evi_pool_worker_t curr = pool->worker_head;
			pool->worker_head = curr->next;

			ev_mutex_lock(curr->lock);
			curr->kys = true;
			ev_thread_cancel(curr->thread);
			ev_cond_broadcast(curr->cond);
			ev_mutex_unlock(curr->lock);

			ev_thread_free_join(curr->thread);

			ev_cond_free(curr->cond);
			ev_mutex_free(curr->lock);
			free(curr);
		}

		pool->worker_head = NULL;
	}
#else
	static ev_code_t evi_pool_exec(evi_pool_t pool, ev_req_t req, ev_worker_t worker, void *args) {
		(void)pool;
		evi_req_begin(req, _evi_pool_cancelcb);
		evi_req_end(req, worker(args));
	}
	static void evi_pool_init(evi_pool_t pool) {
		(void)pool;
	}
	static void evi_pool_free(evi_pool_t pool) {
		(void)pool;
	}
#endif
