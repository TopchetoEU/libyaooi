#ifndef EV_TIME_H
#define EV_TIME_H

#include <stdint.h>

#include <ev/conf.h>

typedef struct {
	int64_t sec;
	uint32_t nsec;
} ev_time_t;

// Adds the two times together
ev_time_t ev_timeadd(ev_time_t a, ev_time_t b);
// Subtracts the two times
ev_time_t ev_timesub(ev_time_t a, ev_time_t b);
// Compares both timestamps, in a strcmp fashion
int ev_timecmp(ev_time_t a, ev_time_t b);
// Converts the time to a millisecond count
int64_t ev_timems(ev_time_t time);

typedef enum {
	EV_CLOCK_REALTIME,
	EV_CLOCK_MONOTIME,
	EV_CLOCK_CPUTIME,
} ev_clock_t;

// Gets the current monotonic time
ev_time_t ev_time(ev_clock_t clock);
// Blocks until the monotonic time is greater than `until`
void ev_timesleep(ev_time_t until);

#endif
