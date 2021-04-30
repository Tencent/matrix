//
// Created by Yves on 2020-03-11.
//

#ifndef LIBMATRIX_HOOK_THREADTRACE_H
#define LIBMATRIX_HOOK_THREADTRACE_H


#include <pthread.h>
#include "PthreadHook.h"
#include "HookCommon.h"


namespace thread_trace {

    //void add_hook_parent_thread_name(const char *__regex_str);
    void thread_trace_init();

    void add_hook_thread_name(const char *__regex_str);

    void pthread_dump(const char *path = "/sdcard/pthread_hook.log");

    void pthread_dump_json(const char *path = "/sdcard/pthread_hook.log");

    void pthread_hook_on_dlopen(const char *file_name);

    typedef struct {
        pthread_hook::pthread_routine_t wrapped_func;
        pthread_hook::pthread_routine_t origin_func;
        void *origin_args;
    } routine_wrapper_t;

    routine_wrapper_t* wrap_pthread_routine(pthread_hook::pthread_routine_t start_routine, void* args);

    void on_pthread_create(pthread_t* pthread_ptr, pthread_attr_t const* attr, pthread_hook::pthread_routine_t start_routine, void* args);

    void on_pthread_setname_np(pthread_t pthread, const char *name);

}


#endif //LIBMATRIX_HOOK_THREADTRACE_H
