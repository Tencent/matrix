//
// Created by Yves on 2020-03-11.
//

#ifndef LIBMATRIX_HOOK_PTHREADHOOK_H
#define LIBMATRIX_HOOK_PTHREADHOOK_H

#include <pthread.h>
#include "HookCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

//void add_hook_parent_thread_name(const char *__regex_str);
void pthread_hook_init();

void add_hook_thread_name(const char *__regex_str);

void pthread_dump(const char *path = "/sdcard/pthread_hook.log");

void pthread_dump_json(const char *path = "/sdcard/pthread_hook.log");

void pthread_hook_on_dlopen(const char *file_name);

void enable_quicken_unwind(const bool enable);

inline int wrap_pthread_getname_np(pthread_t pthread, char *buf, size_t n);

DECLARE_HOOK_ORIG(int, pthread_create, pthread_t* pthread_ptr, pthread_attr_t const* attr, void* (*start_routine)(void*), void* arg);

DECLARE_HOOK_ORIG(int, pthread_setname_np, pthread_t pthread, const char* name);

#ifdef __cplusplus
}
#endif

#endif //LIBMATRIX_HOOK_PTHREADHOOK_H
