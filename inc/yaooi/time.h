// libyaooi, Copyright (C) 2025-2026 topchetoeu, see LICENSE for full LGPL text

#ifndef YO_TIME_H
#define YO_TIME_H

#include <stdint.h>

#include <yaooi/conf.h>

typedef struct {
	int64_t sec;
	uint32_t nsec;
} yo_time_t;

// Adds the two times together
yo_time_t yo_timeadd(yo_time_t a, yo_time_t b);
// Subtracts the two times
yo_time_t yo_timesub(yo_time_t a, yo_time_t b);
// Compares both timestamps, in a strcmp fashion
int yo_timecmp(yo_time_t a, yo_time_t b);
// Converts the time to a millisecond count
int64_t yo_timems(yo_time_t time);

typedef enum {
	YO_CLOCK_REAL,
	YO_CLOCK_MONO,
	YO_CLOCK_CPU,
} yo_clock_t;

// Gets the current monotonic time
yo_time_t yo_time(yo_clock_t clock);
// Blocks until the monotonic time is greater than `until`
void yo_timesleep(yo_time_t until);

#endif
