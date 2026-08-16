#pragma once

#include <ev/filelist.h>
#include <ev/io.h>

#include "./impl.h"

struct ev_filelist {
	ev_fd_t fd_head;
	ev_dir_t dir_head;
	ev_proc_t proc_head;
};

#define evi_list_fl_slot(node) ((node)->slot)
#define evi_list_fl_next(node) ((node)->next)

struct ev_fd {
	ev_fd_t *slot;
	ev_fd_t next;

	ev_req_t head;
	bool owned;

	evi_fd_impl_t impl;
};

struct ev_dir {
	ev_dir_t *slot;
	ev_dir_t next;

	ev_req_t head;

	struct evi_dir_impl impl;
};

struct ev_proc {
	ev_proc_t *slot;
	ev_proc_t next;

	ev_req_t head;

	struct evi_proc_impl impl;
};
