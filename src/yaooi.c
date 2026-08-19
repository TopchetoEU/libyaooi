// libyaooi, Copyright (C) 2025-2026 topchetoeu, see LICENSE for full LGPL text

#pragma GCC diagnostic ignored "-Wunused-function"

// IWYU pragma: private

#define _GNU_SOURCE

#include <stdint.h>

#include <yaooi/conf.h>
#include <yaooi/errno.h>

// Source files included here for a unity build

#include "./addr.c" // IWYU pragma: export
#include "./queue.c" // IWYU pragma: export
#include "./time.c" // IWYU pragma: export
#include "./impl.c" // IWYU pragma: export
#include "./fallback/async.c" // IWYU pragma: export

const char *yo_strerr(yo_code_t code) {
	switch (code) {
		#define YO_SIGDEF_SWITCH_X(name, code, msg) case code: return msg;
		YO_SIGDEF(YO_SIGDEF_SWITCH_X)
		#undef YO_SIGDEF_SWITCH_X
		default: return "unknown OS-specific error";
	}
}
