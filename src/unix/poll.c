#pragma once

#include "pollish.h"
#define _GNU_SOURCE

#include <stdlib.h>

#include <sys/select.h>
#include <sys/poll.h>

#include <yaooi/conf.h>
#include <yaooi/errno.h>
#include <yaooi/time.h>
#include <yaooi/queue.h>

#include "./poll.h" // IWYU pragma: export

#include "./pollish.c"

static uint64_t _yoi_poll_subms_diff(yo_time_t timeout) {
	yo_time_t diff = yo_timesub(yo_time(YO_CLOCK_MONOTIME), timeout);

	if (diff.sec != 0) return 0;
	if (diff.nsec > 1000000) return 0;
	return diff.nsec;
}

static yo_code_t yoi_pl_impl_setmask(yo_queue_t queue, void *udata, int fd, yoi_pl_evn_mask_t mask) {
	if (mask == 0) {
		for (yo_poll_node_t i = queue->impl.head; i; i = i->next) {
			if (i->fd == fd) {
				yoi_dlist_del(poll, i);
				break;
			}
		}

		return YO_OK;
	}
	else {
		yo_poll_node_t i;
		for (i = queue->impl.head; i; i = i->next) {
			if (i->fd == fd) break;
		}

		if (!i) {
			i = malloc(sizeof *i);
			if (!i) return YO_ENOMEM;

			i->fd = fd;

			yoi_dlist_add(poll, queue->impl.head, i);
		}

		i->evn = mask;
		i->udata = udata;

		return YO_OK;
	}
}
static bool _yoi_poll_addfd(yo_queue_t queue, yo_poll_node_t node, size_t *pn) {
	if (*pn >= queue->impl.fds_cap) {
		queue->impl.fds_cap *= 2;
		if (!queue->impl.fds_cap) queue->impl.fds_cap = 16;

		struct pollfd *new_fds = realloc(queue->impl.fds, sizeof *queue->impl.fds * queue->impl.fds_cap);
		if (!new_fds) return false;
		queue->impl.fds = new_fds;
	}

	struct pollfd *target = &queue->impl.fds[*pn];
	(*pn)++;

	memset(&queue->impl.fds[*pn], 0 ,sizeof *queue->impl.fds);

	target->fd = node->fd;
	target->events = 0;

	if (node->evn & YOI_PL_READABLE) target->events |= POLLIN;
	if (node->evn & YOI_PL_WRITABLE) target->events |= POLLOUT;

	node->pollfd_i = target - queue->impl.fds;

	return true;
}

static size_t _yoi_poll_wrapper(yo_queue_t queue, size_t fd_n, const yo_time_t *deadline) {
	while (true) {
		int code;
		if (deadline) {
			yo_time_t diff = yo_timesub(*deadline, yo_time(YO_CLOCK_MONOTIME));

			#ifdef YO_USE_LINUX
				if (diff.sec < 0) {
					diff.sec = 0;
					diff.nsec = 0;
				}
				code = ppoll(queue->impl.fds, fd_n, &(struct timespec) { .tv_sec = diff.sec, .tv_nsec = diff.nsec }, NULL);
			#else
				int64_t diff_ms = yo_timems(diff);
				if (diff_ms < 0) diff_ms = 0;
				code = poll(queue->impl.fds, fd_n, diff_ms);
			#endif
		}
		else {
			code = poll(queue->impl.fds, fd_n, -1);
		}

		if (code < 0) {
			if (errno == EINTR || errno == EAGAIN) continue;
			assert(false && "poll call failed");
		}

		return code;
	}
}

static yo_code_t yoi_pl_impl_poll(yo_queue_t queue, const yo_time_t *deadline, void **pudata, yoi_pl_evn_mask_t *pready) {
	size_t fd_n = 0;

	for (yo_poll_node_t it = queue->impl.head; it; it = it->next) {
		if (!_yoi_poll_addfd(queue, it, &fd_n)) return YO_ENOMEM;
	}

	_yoi_poll_wrapper(queue, fd_n, deadline);

	for (yo_poll_node_t it = queue->impl.head; it; it = it->next) {
		struct pollfd pollfd = queue->impl.fds[it->pollfd_i];
		yoi_pl_evn_mask_t ready = 0;

		if (pollfd.revents & (POLLIN | POLLERR | POLLHUP)) ready |= YOI_PL_READABLE;
		if (pollfd.revents & (POLLOUT | POLLERR | POLLHUP)) ready |= YOI_PL_WRITABLE;

		if (ready) {
			*pready = ready;
			*pudata = it->udata;
			return YO_OK;
		}
	}

	return YO_ETIMEDOUT;
}

static yo_code_t yoi_queue_impl_init(yo_queue_t queue) {
	queue->impl.fds = NULL;
	queue->impl.fds_cap = 0;
	queue->impl.head = NULL;

	return YO_OK;
}
static yo_code_t yoi_queue_impl_free(yo_queue_t queue) {
	(void)queue;
	return YO_OK;
}
