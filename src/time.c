#pragma once

#include <yaioi/time.h> // IWYU pragma: export

yo_time_t yo_timeadd(yo_time_t a, yo_time_t b) {
	yo_time_t res = { .sec = a.sec + b.sec, .nsec = a.nsec + b.nsec };
	if (res.nsec > 1000000000) {
		res.nsec -= 1000000000;
		res.sec += 1;
	}
	return res;
}
yo_time_t yo_timesub(yo_time_t a, yo_time_t b) {
	if (a.nsec < b.nsec) {
		a.nsec += 1000000000;
		a.sec -= 1;
	}

	yo_time_t res = { .sec = a.sec - b.sec, .nsec = a.nsec - b.nsec };
	if (res.nsec > 1000000000) {
		res.sec += 1;
		res.nsec -= 1000000000;
	}

	return res;
}
int yo_timecmp(yo_time_t a, yo_time_t b) {
	if (a.sec != b.sec) return a.sec < b.sec ? -1 : 1;
	if (a.nsec != b.nsec) return a.nsec < b.nsec ? -1 : 1;
	return 0;
}
int64_t yo_timems(yo_time_t time) {
	return time.sec * 1000 + (time.nsec + 999999) / 1000000;
}
