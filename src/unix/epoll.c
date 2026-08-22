// libyaooi, Copyright (C) 2025-2026 topchetoeu, see LICENSE for full LGPL text

#pragma once

#include <assert.h>
#include <err.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/timerfd.h>

#include <yaooi/conf.h>
#include <yaooi/errno.h>
#include <yaooi/queue.h>
#include <yaooi/time.h>

#include "./epoll.h" // IWYU pragma: export

#include "./impl.h"
#include "./pollish.c"

typedef struct {
	int fd;
	void *udata;
} *_yoi_epoll_udata_t;

yo_code_t yoi_pl_impl_setmask(yo_queue_t queue, void *udata, int fd, yoi_pl_evn_mask_t mask) {
	int realmask = EPOLLRDHUP | EPOLLERR | EPOLLHUP;
	if (mask & YOI_PL_READABLE) realmask |= EPOLLIN;
	if (mask & YOI_PL_WRITABLE) realmask |= EPOLLOUT;

	struct epoll_event evn = {
		.data.ptr = udata,
		.events = realmask,
	};

	if (mask == 0) {
		if (epoll_ctl(queue->impl.epoll_fd, EPOLL_CTL_DEL, fd, &evn) == 0) return YO_OK;
		if (errno == ENOENT) return YO_OK;
		return yoi_unix_conv_errno(errno);
	}
	else {
		if (epoll_ctl(queue->impl.epoll_fd, EPOLL_CTL_MOD, fd, &evn) == 0) return YO_OK;
		if (errno == ENOENT) {
			if (epoll_ctl(queue->impl.epoll_fd, EPOLL_CTL_ADD, fd, &evn) == 0) return YO_OK;
		}

		return yoi_unix_conv_errno(errno);
	}
}

static yo_code_t yoi_pl_impl_poll(yo_queue_t queue, const yo_time_t *deadline, void **pudata, yoi_pl_evn_mask_t *pready) {
	struct epoll_event evn = { 0 };

	if (deadline) {
		yo_time_t now = yo_time(YO_CLOCK_MONO);
		if (yo_timecmp(now, *deadline) > 0) return YO_ETIMEDOUT;

		timerfd_settime(queue->impl.timer_fd, TFD_TIMER_ABSTIME, &(struct itimerspec) {
			.it_value = { deadline->sec, deadline->nsec },
			.it_interval = { 0, 0 }
		}, NULL);
	}
	else {
		timerfd_settime(queue->impl.timer_fd, 0, &(struct itimerspec) {
			.it_value = { 0, 0 },
			.it_interval = { 0, 0 }
		}, NULL);
	}

	int n;
	while ((n = epoll_wait(queue->impl.epoll_fd, &evn, 1, -1)) < 0) {
		if (errno != EINTR) err(1, "failed to poll");
	}
	if (n == 0) {
		// if (deadline) return YO_ETIMEDOUT;
		return YO_ETIMEDOUT;
	}

	if (evn.data.fd == queue->impl.timer_fd) {
		assert(deadline != NULL && "timer returned without a timeout");
		char buff[8];
		read(evn.data.fd, buff, sizeof buff);
		return YO_ETIMEDOUT;
	}

	yoi_pl_evn_mask_t mask = 0;

	if (evn.events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLIN)) mask |= YOI_PL_READABLE;
	if (evn.events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLOUT)) mask |= YOI_PL_WRITABLE;

	*pudata = evn.data.ptr;
	*pready = mask;
	return YO_OK;
}

static yo_code_t yoi_queue_impl_init(yo_queue_t queue) {
	yo_code_t code;

	queue->impl.timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
	if (queue->impl.timer_fd < 0) { code = yoi_unix_conv_errno(errno); goto err_timer; }

	queue->impl.epoll_fd = epoll_create(16);
	if (queue->impl.epoll_fd < 0) { code = yoi_unix_conv_errno(errno); goto err_epoll; }

	if (epoll_ctl(queue->impl.epoll_fd, EPOLL_CTL_ADD, queue->impl.timer_fd, &(struct epoll_event) {
		.events = EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLIN,
		.data.fd = queue->impl.timer_fd,
	}) < 0) goto err_epoll_ctl;

	if ((code = yoi_pl_init(&queue->impl.pl, queue)) != YO_OK) goto err_pl;

	return YO_OK;

err_pl:
err_epoll_ctl:
	close(queue->impl.epoll_fd);
err_epoll:
	close(queue->impl.timer_fd);
err_timer:
	return yoi_unix_conv_errno(errno);
}
static yo_code_t yoi_queue_impl_free(yo_queue_t queue) {
	yo_code_t code;

	if ((code = yoi_pl_init(&queue->impl.pl, queue)) != YO_OK) return code;

	close(queue->impl.epoll_fd);
	close(queue->impl.timer_fd);
	return YO_OK;
}
