#pragma once

#include <ev/conf.h>
#include <ev/errno.h>

#include "./pollish.h"

struct evi_queue_impl {
	evi_pl_s pl;
	int epoll_fd;
	int timer_fd;
};

struct evi_async_fd {
	unsigned read, write;
};
