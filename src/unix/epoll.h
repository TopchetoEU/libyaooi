#pragma once

#include <yaioi/conf.h>
#include <yaioi/errno.h>

#include "./pollish.h" // IWYU pragma: export

typedef struct {
	// All pollish ioq backends must have this field
	yoi_pl_s pl;
	int epoll_fd;
	int timer_fd;
} yoi_queue_impl_t;

static yo_code_t yoi_queue_impl_init(yo_queue_t queue);
static yo_code_t yoi_queue_impl_free(yo_queue_t queue);
