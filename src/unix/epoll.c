#pragma once

#include <assert.h>
#include <err.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/timerfd.h>

#include <ev/conf.h>
#include <ev/errno.h>
#include <ev/queue.h>
#include <ev/time.h>

#include "./epoll.h" // IWYU pragma: export

#include "./impl.h"
#include "./pollish.c"
#include "./pollish.c"

typedef struct {
	int fd;
	void *udata;
} *_evi_epoll_udata_t;

ev_code_t evi_pl_impl_setmask(ev_queue_t queue, void *udata, int fd, evi_pl_evn_mask_t mask) {
	int realmask = EPOLLRDHUP | EPOLLERR | EPOLLHUP;
	if (mask & EVI_PL_READABLE) realmask |= EPOLLIN;
	if (mask & EVI_PL_WRITABLE) realmask |= EPOLLOUT;

	struct epoll_event evn = {
		.data.ptr = udata,
		.events = realmask,
	};

	if (mask == 0) {
		if (epoll_ctl(queue->impl.epoll_fd, EPOLL_CTL_DEL, fd, &evn) == 0) return EV_OK;
		if (errno == ENOENT) return EV_OK;
		return evi_unix_conv_errno(errno);
	}
	else {
		if (epoll_ctl(queue->impl.epoll_fd, EPOLL_CTL_MOD, fd, &evn) == 0) return EV_OK;
		if (errno == ENOENT) {
			if (epoll_ctl(queue->impl.epoll_fd, EPOLL_CTL_ADD, fd, &evn) == 0) return EV_OK;
		}

		return evi_unix_conv_errno(errno);
	}
}

static ev_code_t evi_pl_impl_poll(ev_queue_t queue, const ev_time_t *deadline, void **pudata, evi_pl_evn_mask_t *pready) {
	struct epoll_event evn = { 0 };

	if (deadline) {
		ev_time_t now = ev_time(EV_CLOCK_MONOTIME);
		if (ev_timecmp(now, *deadline) > 0) return EV_ETIMEDOUT;

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
		// if (deadline) return EV_ETIMEDOUT;
		return EV_ETIMEDOUT;
	}

	if (evn.data.fd == queue->impl.timer_fd) {
		assert(deadline != NULL && "timer returned without a timeout");
		return EV_ETIMEDOUT;
	}

	evi_pl_evn_mask_t mask = 0;

	if (evn.events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLIN)) mask |= EVI_PL_READABLE;
	if (evn.events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLOUT)) mask |= EVI_PL_WRITABLE;

	*pudata = evn.data.ptr;
	*pready = mask;
	return EV_OK;
}

static ev_code_t evi_queue_impl_init(ev_queue_t queue) {
	ev_code_t code;

	queue->impl.timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
	if (queue->impl.timer_fd < 0) { code = evi_unix_conv_errno(errno); goto err_timer; }

	queue->impl.epoll_fd = epoll_create(16);
	if (queue->impl.epoll_fd < 0) { code = evi_unix_conv_errno(errno); goto err_epoll; }

	if (epoll_ctl(queue->impl.epoll_fd, EPOLL_CTL_ADD, queue->impl.timer_fd, &(struct epoll_event) {
		.events = EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLIN,
		.data.fd = queue->impl.timer_fd,
	}) < 0) goto err_epoll_ctl;

	if ((code = evi_pl_init(&queue->impl.pl, queue)) != EV_OK) goto err_pl;

	return EV_OK;

err_pl:
err_epoll_ctl:
	close(queue->impl.epoll_fd);
err_epoll:
	close(queue->impl.timer_fd);
err_timer:
	return evi_unix_conv_errno(errno);
}
static ev_code_t evi_queue_impl_free(ev_queue_t queue) {
	ev_code_t code;

	if ((code = evi_pl_init(&queue->impl.pl, queue)) != EV_OK) return code;

	close(queue->impl.epoll_fd);
	close(queue->impl.timer_fd);
	return EV_OK;
}
