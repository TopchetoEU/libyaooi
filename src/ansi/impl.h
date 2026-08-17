#pragma once

#include <stdio.h>

#include <ev/io.h>

#include "../fallback/queue.h" // IWYU pragma: export
#include "ev/queue.h"

static void evi_ansi_mkfd(ev_fd_t res, FILE *f);
static bool evi_ansi_mkat(ev_fd_t res, const char *path);

static int evi_ansi_isfd(ev_fd_t fd);

static ev_code_t evi_ansi_conv_errno(int err, ev_code_t fallback);

struct ev_fd {
	ev_req_t head;
	bool owned;

	enum {
		EVI_ANSI_FILE,
		EVI_ANSI_AT,
	} kind;
	union {
		FILE *file;
		char *at;
	};
};
struct ev_dir {
};
struct ev_proc {
};
struct ev_enviter {
	char **enviter;
};

typedef struct {
} evi_req_ioq_t;
