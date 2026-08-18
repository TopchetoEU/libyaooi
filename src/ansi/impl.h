#pragma once

#include <stdio.h>

#include <yaooi/io.h>

#include "../fallback/queue.h" // IWYU pragma: export

static void yoi_ansi_mkfd(yo_fd_t res, FILE *f);
static bool yoi_ansi_mkat(yo_fd_t res, const char *path);

static int yoi_ansi_isfd(yo_fd_t fd);

static yo_code_t yoi_ansi_conv_errno(int err, yo_code_t fallback);

struct yo_fd {
	bool owned;

	enum {
		YOI_ANSI_FILE,
		YOI_ANSI_AT,
	} kind;
	union {
		FILE *file;
		char *at;
	};
};
struct yo_dir {
};
struct yo_proc {
};
struct yo_enviter {
	char **enviter;
};

typedef struct {
} yoi_req_ioq_t;
