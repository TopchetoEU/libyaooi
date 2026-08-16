#pragma once

#include <ev/conf.h>
#include <ev/errno.h>

#include "./pollish.h" // IWYU pragma: export

typedef struct {
	// All pollish ioq backends must have this field
	evi_pl_s pl;
	int epoll_fd;
	int timer_fd;
} evi_queue_impl_t;

static ev_code_t evi_queue_impl_init(ev_queue_t queue);
static ev_code_t evi_queue_impl_free(ev_queue_t queue);
