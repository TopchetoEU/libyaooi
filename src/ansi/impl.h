#pragma once

#include <stdio.h>

#include <ev/io.h>

#include "../fallback/queue.h" // IWYU pragma: export

static void evi_generic_mkfd(ev_filelist_t fl, ev_fd_t res, FILE *f);
static bool evi_generic_mkat(ev_filelist_t fl, ev_fd_t res, const char *path);

static int evi_generic_isfd(ev_fd_t fd);

static ev_code_t evi_generic_conv_errno(int err, ev_code_t fallback);

typedef struct {
	enum {
		EVI_ANSI_FILE,
		EVI_ANSI_AT,
	} kind;
	union {
		FILE *file;
		char *at;
	};
} evi_fd_impl_t;
typedef struct {
} evi_fd_ioq_t;

typedef struct {
} evi_req_ioq_t;

typedef struct {
} evi_dir_impl_t;

typedef struct {
} evi_proc_impl_t;

struct ev_enviter {
	char **enviter;
};
