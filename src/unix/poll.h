#pragma once

#include <ev/conf.h>
#include <ev/errno.h>

#include "./pollish.h"

typedef struct ev_poll_node {
	struct ev_poll_node **slot;
	struct ev_poll_node *next;
	#define evi_list_poll_next(node) (node)->next
	#define evi_list_poll_slot(node) (node)->slot

	int fd;
	void *udata;
	evi_pl_evn_mask_t evn;

	size_t pollfd_i;
} *ev_poll_node_t;

typedef struct {
	struct pollfd *fds;
	size_t fds_cap;

	ev_poll_node_t head;

	// All pollish ioq backends must have this field
	evi_pl_s pl;
} evi_queue_impl_t;

static ev_code_t evi_queue_impl_init(ev_queue_t queue);
static ev_code_t evi_queue_impl_free(ev_queue_t queue);
