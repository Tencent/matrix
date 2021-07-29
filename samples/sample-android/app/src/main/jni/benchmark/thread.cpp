// ported from https://github.com/mjansson/rpmalloc-benchmark
#include "thread.h"

#ifdef _MSC_VER
#  define ATTRIBUTE_NORETURN
#else
#  define ATTRIBUTE_NORETURN __attribute__((noreturn))
#endif

#ifdef _WIN32
#  include <windows.h>
#  include <process.h>

static unsigned __stdcall
thread_entry(void* argptr) {
	thread_arg* arg = argptr;
	arg->fn(arg->arg);
	return 0;
}

#else
#  include <time.h>
#  include <pthread.h>
#  include <sched.h>

#ifdef __cplusplus
extern "C" {
#endif

static void*
thread_entry(void* argptr) {
	thread_arg* arg = (thread_arg *)argptr;
	arg->fn(arg->arg);
	return 0;
}

#endif

uintptr_t
thread_run(thread_arg* arg) {
#ifdef _WIN32
	return _beginthreadex(0, 0, thread_entry, arg, 0, 0);
#else
	pthread_t id = 0;
	int err = pthread_create(&id, 0, thread_entry, arg);
	if (err)
		return 0;
	return (uintptr_t)id;
#endif
}

void ATTRIBUTE_NORETURN
thread_exit(void* value) {
#ifdef _WIN32

#else
	pthread_exit(value);
#endif
}

void
thread_join(uintptr_t handle) {
	if (!handle)
		return;
#ifdef _WIN32
	WaitForSingleObject((HANDLE)handle, INFINITE);
	CloseHandle((HANDLE)handle);
#else
	void* result = 0;
	pthread_join((pthread_t)handle, &result);
#endif
}

void
thread_sleep(int milliseconds) {
#ifdef _WIN32
	SleepEx(milliseconds, 1);
#else
	struct timespec ts;
	ts.tv_sec  = milliseconds / 1000;
	ts.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
	nanosleep(&ts, 0);
#endif
}

void
thread_yield(void) {
#ifdef _WIN32
	Sleep(0);
	_ReadWriteBarrier();
#else
	sched_yield();
	__sync_synchronize();
#endif
}

void
thread_fence(void) {
#ifdef _WIN32
	_ReadWriteBarrier();
#else
	__sync_synchronize();
#endif
}

#ifdef __cplusplus
}
#endif