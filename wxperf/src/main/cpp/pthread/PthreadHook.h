//
// Created by Yves on 2020-03-11.
//

#ifndef LIBWXPERF_JNI_PTHREADHOOK_H
#define LIBWXPERF_JNI_PTHREADHOOK_H

#include <pthread.h>
#include "HookCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

//void add_hook_parent_thread_name(const char *__regex_str);
void pthread_hook_init();

void add_hook_thread_name(const char *__regex_str);

void pthread_dump(const char *__path = "/sdcard/pthread_hook.log");

char * pthread_dump_json(const char *__path = "/sdcard/pthread_hook.log");

void pthread_hook_on_dlopen(const char *__file_name);

inline int wrap_pthread_getname_np(pthread_t __pthread, char *__buf, size_t __n);

DECLARE_HOOK_ORIG(int, pthread_create, pthread_t* __pthread_ptr, pthread_attr_t const* __attr, void* (*__start_routine)(void*), void* __arg);

DECLARE_HOOK_ORIG(int, pthread_setname_np, pthread_t __pthread, const char* __name);

#ifdef __cplusplus
}
#endif

#endif //LIBWXPERF_JNI_PTHREADHOOK_H
