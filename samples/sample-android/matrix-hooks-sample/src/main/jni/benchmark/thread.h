#ifndef LIBWXPERF_JNI_THREAD_H
#define LIBWXPERF_JNI_THREAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct thread_arg {
	void(*fn)(void*);
	void* arg;
};
typedef struct thread_arg thread_arg;

extern uintptr_t
thread_run(thread_arg* arg);

extern void
thread_exit(void* value);

extern void
thread_join(uintptr_t handle);

extern void
thread_sleep(int milliseconds);

extern void
thread_yield(void);

extern void
thread_fence(void);

#ifdef __cplusplus
}
#endif

#endif //LIBWXPERF_JNI_THREAD_H