#pragma GCC diagnostic ignored "-Wunused-function"

// IWYU pragma: private

#define _GNU_SOURCE

#include <stdint.h>

#include <ev/conf.h>
#include <ev/errno.h>

// Source files included here for a unity build

#include "./addr.c" // IWYU pragma: export
#include "./queue.c" // IWYU pragma: export
#include "./time.c" // IWYU pragma: export
#include "./impl.c" // IWYU pragma: export
#include "./fallback/async.c" // IWYU pragma: export

const char *ev_strerr(ev_code_t code) {
	switch (code) {
		#define EV_SIGDEF_SWITCH_X(name, code, msg) case code: return msg;
		EV_SIGDEF(EV_SIGDEF_SWITCH_X)
		#undef EV_SIGDEF_SWITCH_X
		default: return "unknown OS-specific error";
	}
}
