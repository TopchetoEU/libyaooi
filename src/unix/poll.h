#pragma once

#include <yaooi/conf.h>
#include <yaooi/errno.h>

#include "./pollish.h"

typedef struct yo_poll_node {
	struct yo_poll_node **slot;
	struct yo_poll_node *next;
	#define yoi_list_poll_next(node) (node)->next
	#define yoi_list_poll_slot(node) (node)->slot

	int fd;
	void *udata;
	yoi_pl_evn_mask_t evn;

	size_t pollfd_i;
} *yo_poll_node_t;

typedef struct {
	struct pollfd *fds;
	size_t fds_cap;

	yo_poll_node_t head;

	// All pollish ioq backends must have this field
	yoi_pl_s pl;
} yoi_queue_impl_t;

static yo_code_t yoi_queue_impl_init(yo_queue_t queue);
static yo_code_t yoi_queue_impl_free(yo_queue_t queue);
