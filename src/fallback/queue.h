#pragma once

#include <yaooi/conf.h>

#include "./queue.h" // IWYU pragma: export

#include "../utils/multithread.h"

typedef struct {
	#ifdef YO_USE_MULTITHREAD
		yo_cond_t cond;
	#endif
} yoi_queue_impl_t;

