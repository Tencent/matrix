// ported from https://github.com/mjansson/rpmalloc-benchmark
#include "timer.h"

#if defined(_WIN32)
#  include <Windows.h>
#elif defined(__APPLE__)
#  include <mach/mach_time.h>
static mach_timebase_info_data_t _time_info;
static void
absolutetime_to_nanoseconds(uint64_t mach_time, uint64_t* clock) {
	*clock = (uint64_t)(mach_time * _time_info.numer / _time_info.denom);
}
#else
#  include <time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

static uint64_t _time_freq;

int
timer_initialize(void) {
#if defined(_WIN32)
	uint64_t unused;
	if (!QueryPerformanceFrequency((LARGE_INTEGER*)&_time_freq) ||
		!QueryPerformanceCounter((LARGE_INTEGER*)&unused))
		return -1;
#elif defined(__APPLE__)
	if (mach_timebase_info(&_time_info))
		return -1;
	_time_freq = 1000000000LL;
#else
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 0 };
	if (clock_gettime(CLOCK_MONOTONIC, &ts))
		return -1;
	_time_freq = 1000000000LL;
#endif
	return 0;
}

uint64_t
timer_current(void) {
#if defined(_WIN32)
	uint64_t curclock;
	QueryPerformanceCounter((LARGE_INTEGER*)&curclock);
	return curclock;
#elif defined(__APPLE__)
	uint64_t curclock = 0;
	absolutetime_to_nanoseconds(mach_absolute_time(), &curclock);
	return curclock;
#else
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 0 };
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((uint64_t)ts.tv_sec * 1000000000LL) + (uint64_t)ts.tv_nsec;
#endif
}

uint64_t
timer_ticks_per_second(void) {
	return _time_freq;
}

double
timer_ticks_to_seconds(uint64_t ticks) {
	uint64_t tps = timer_ticks_per_second();
	return (double)ticks / (double)tps;
}

#ifdef __cplusplus
}
#endif