#pragma once

#include <ev/conf.h>

#include "./queue.h" // IWYU pragma: export

#include "../utils/multithread.h"

typedef struct {
	#ifdef EV_USE_MULTITHREAD
		ev_cond_t cond;
	#endif
} evi_queue_impl_t;

