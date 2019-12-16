//
// Created by Yves on 2019-12-16.
//

#ifndef LIBWXPERF_JNI_MEMORYHOOK_DEF_H
#define LIBWXPERF_JNI_MEMORYHOOK_DEF_H

#define GET_CALLER_ADDR(__caller_addr) \
    void * __caller_addr = __builtin_return_address(0)

#define HANDLER_FUNC_NAME(fn_name) h_##fn_name
#define ORIGINAL_FUNC_NAME(fn_name) orig_##fn_name

#define FUNC_TYPE(sym) fn_##sym##_t
#define ORIGINAL_FUNC_PTR(sym) FUNC_TYPE(sym) ORIGINAL_FUNC_NAME(sym)

#define CALL_ORIGIN_FUNC_RET(retType, ret ,sym, params...) \
    if (!ORIGINAL_FUNC_NAME(sym)) { \
        void *handle = dlopen("libc++_shared.so", RTLD_LAZY); \
        if (handle) { \
            ORIGINAL_FUNC_NAME(sym) = (FUNC_TYPE(sym))dlsym(handle, #sym); \
        } \
    } \
    retType ret = ORIGINAL_FUNC_NAME(sym)(params)

#define CALL_ORIGIN_FUNC_VOID(sym, params...) \
    if (!ORIGINAL_FUNC_NAME(sym)) { \
        void *handle = dlopen("libc++_shared.so", RTLD_LAZY); \
        if (handle) { \
            ORIGINAL_FUNC_NAME(sym) = (FUNC_TYPE(sym))dlsym(handle, #sym); \
        } \
    } \
    ORIGINAL_FUNC_NAME(sym)(params)

#define DECLARE_HOOK_ORIG(ret, sym, params...) \
    typedef ret (*FUNC_TYPE(sym))(params); \
    extern ORIGINAL_FUNC_PTR(sym); \
    ret HANDLER_FUNC_NAME(sym)(params);


#define DEFINE_HOOK_FUN(ret, sym, params...) \
    ORIGINAL_FUNC_PTR(sym); \
    ret HANDLER_FUNC_NAME(sym)(params)

#define DO_HOOK_ACQUIRE(p, size) \
    GET_CALLER_ADDR(caller); \
    on_acquire_memory(caller, p, size);

#define DO_HOOK_RELEASE(p) \
    on_release_memory(p)

//#define DO_HOOK_RELEASE(sym, p, params...) \
//    GET_CALLER_ADDR(caller); \
//    on_release_memory(caller, p); \
//    ORIGINAL_FUNC_NAME(sym)(p, params);

#endif //LIBWXPERF_JNI_MEMORYHOOK_DEF_H
