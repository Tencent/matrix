//
// Created by tomystang on 2019-08-06.
//

#include <cstddef>
#include <cstdlib>
#include <unistd.h>
#include <dlfcn.h>
#include <new>

#define DEFINE_WRAP(ret, sym, params...) \
    typedef ret (*fn_##sym##_t)(params); \
    fn_##sym##_t original_##sym = nullptr; \
    ret __wrap_##sym(params)

#define INIT_ORIGINAL_FN(sym) \
    do { \
        if (original_##sym == nullptr) { \
            original_##sym = (fn_##sym##_t) dlsym(RTLD_NEXT, #sym); \
        } \
    } while (0)


extern "C" {
    DEFINE_WRAP(void*, malloc, size_t size) {
        INIT_ORIGINAL_FN(malloc);
        return original_malloc(size);
    }

    DEFINE_WRAP(void*, calloc, size_t item_count, size_t item_size) {
        INIT_ORIGINAL_FN(calloc);
        return original_calloc(item_count, item_size);
    }

    DEFINE_WRAP(void*, realloc, void* ptr, size_t size) {
        INIT_ORIGINAL_FN(realloc);
        return original_realloc(ptr, size);
    }

    DEFINE_WRAP(void, free, void* ptr) {
        INIT_ORIGINAL_FN(free);
        original_free(ptr);
    }

    DEFINE_WRAP(void*, memalign, size_t alignment, size_t byte_count) {
        INIT_ORIGINAL_FN(memalign);
        return original_memalign(alignment, byte_count);
    }

    DEFINE_WRAP(char*, strdup, char* str) {
        INIT_ORIGINAL_FN(strdup);
        return original_strdup(str);
    }

    DEFINE_WRAP(char*, strndup, char* str, size_t length) {
        INIT_ORIGINAL_FN(strndup);
        return original_strndup(str, length);
    }

    DEFINE_WRAP(void*, _Znwj, size_t size) {
        INIT_ORIGINAL_FN(_Znwj);
        return original__Znwj(size);
    }

    DEFINE_WRAP(void*, _ZnwjRKSt9nothrow_t, size_t size, const std::nothrow_t& nothrow_value) {
        INIT_ORIGINAL_FN(_ZnwjRKSt9nothrow_t);
        return original__ZnwjRKSt9nothrow_t(size, nothrow_value);
    }

    DEFINE_WRAP(void*, _Znaj, size_t size) {
        INIT_ORIGINAL_FN(_Znaj);
        return original__Znaj(size);
    }

    DEFINE_WRAP(void*, _ZnajRKSt9nothrow_t, size_t size, const std::nothrow_t& nothrow_value) {
        INIT_ORIGINAL_FN(_ZnajRKSt9nothrow_t);
        return original__ZnajRKSt9nothrow_t(size, nothrow_value);
    }

    DEFINE_WRAP(void, _ZdlPv, void* ptr) {
        INIT_ORIGINAL_FN(_ZdlPv);
        return original__ZdlPv(ptr);
    }

    DEFINE_WRAP(void, _ZdlPvRKSt9nothrow_t, void* ptr, const std::nothrow_t& nothrow_value) {
        INIT_ORIGINAL_FN(_ZdlPvRKSt9nothrow_t);
        return original__ZdlPvRKSt9nothrow_t(ptr, nothrow_value);
    }

    DEFINE_WRAP(void, _ZdlPvj, void* ptr, size_t size) {
        INIT_ORIGINAL_FN(_ZdlPvj);
        return original__ZdlPvj(ptr, size);
    }

    DEFINE_WRAP(void, _ZdaPv, void* ptr) {
        INIT_ORIGINAL_FN(_ZdaPv);
        return original__ZdaPv(ptr);
    }

    DEFINE_WRAP(void, _ZdaPvRKSt9nothrow_t, void* ptr, const std::nothrow_t& nothrow_value) {
        INIT_ORIGINAL_FN(_ZdaPvRKSt9nothrow_t);
        return original__ZdaPvRKSt9nothrow_t(ptr, nothrow_value);
    }

    DEFINE_WRAP(void, _ZdaPvj, void* ptr, size_t size) {
        INIT_ORIGINAL_FN(_ZdaPvj);
        return original__ZdaPvj(ptr, size);
    }
}