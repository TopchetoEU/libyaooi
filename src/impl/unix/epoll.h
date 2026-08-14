#pragma once

#include <ev/conf.h>
#include <ev/errno.h>

#include "./pollish.h" // IWYU pragma: export

typedef struct {
	evi_pl_s pl;
	int epoll_fd;
	int timer_fd;
} evi_queue_impl_t;

typedef struct {
	unsigned read, write;
} evi_async_fd_t;
