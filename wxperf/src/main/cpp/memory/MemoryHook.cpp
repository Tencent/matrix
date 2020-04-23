//
// Created by Yves on 2019-08-08.
//

#include <malloc.h>
#include <dlfcn.h>
#include <unistd.h>
#include <unordered_map>
#include <set>
#include <random>
#include <xhook.h>
#include <sstream>
#include <cxxabi.h>
#include <sys/mman.h>
#include "MemoryHook.h"
#include "lock.h"
#include "StackTrace.h"
#include "utils.h"
#include "unwindstack/Unwinder.h"

#define ORIGINAL_LIB "libc++_shared.so"

extern "C" typedef struct {
    size_t size;
    void *caller;
    uint64_t stack_hash;
} ptr_meta_t;

extern "C" typedef struct {
    size_t total_size;
    std::set<void *> pointers;
} caller_meta_t;

extern "C" typedef struct {
    size_t size;
    std::vector<unwindstack::FrameData> *p_stacktraces;
} stack_meta_t;

std::map<void *, ptr_meta_t> m_ptr_meta;
std::map<void *, caller_meta_t> m_caller_meta;
std::map<uint64_t, stack_meta_t> m_stack_meta;

std::map<void *, ptr_meta_t> m_mmap_ptr_meta;
std::map<void *, caller_meta_t> m_mmap_caller_meta;
std::map<uint64_t, stack_meta_t> m_mmap_stack_meta;

bool is_stacktrace_enabled = false;

size_t m_sample_size_min = 0;
size_t m_sample_size_max = 0;

double m_sampling = 1;

void enable_stacktrace(bool __enable) {
    is_stacktrace_enabled = __enable;
}

void set_sample_size_range(size_t __min, size_t __max) {
    m_sample_size_min = __min;
    m_sample_size_max = __max;
}

void set_sampling(double __sampling) {
    m_sampling = __sampling;
}

static inline void record_acquire_mem_unsafe(void *__caller,
                                             void *__ptr,
                                             size_t __byte_count,
                                             std::map<void *, ptr_meta_t> &ptr_metas,
                                             std::map<void *, caller_meta_t> &caller_metas,
                                             std::map<uint64_t, stack_meta_t> &stack_metas) {
    if (ptr_metas.count(__ptr) && ptr_metas.at(__ptr).size == __byte_count) { // 检查是否重复记录同一个指针
        LOGD("Yves-debug", "skip for dup record");
        return;
    }
//    LOGD("Yves-debug", "on acquire ptr = %p", __ptr);
    ptr_meta_t &ptr_meta = ptr_metas[__ptr];
    ptr_meta.size = __byte_count;
    ptr_meta.caller = __caller;

//    LOGD("Yves-debug", "caller = %p", __caller);
    if (__caller) {
        caller_meta_t &caller_meta = caller_metas[__caller];
        caller_meta.pointers.insert(__ptr);
        caller_meta.total_size += __byte_count;
    }

    if (is_stacktrace_enabled &&
        (m_sample_size_min == 0 || __byte_count >= m_sample_size_min) &&
        (m_sample_size_max == 0 || __byte_count <= m_sample_size_max)) {

        int r = rand();
        if (r > m_sampling * RAND_MAX) {
            return;
        }

        auto ptr_stack_frames = new std::vector<unwindstack::FrameData>;
        unwindstack::do_unwind(*ptr_stack_frames);

        if (!ptr_stack_frames->empty()) {
            uint64_t stack_hash = hash_stack_frames(*ptr_stack_frames);
            ptr_meta.stack_hash = stack_hash;
            stack_meta_t &stack_meta = stack_metas[stack_hash];
            stack_meta.size += __byte_count;

            if (stack_meta.p_stacktraces) { // 相同的堆栈只记录一个
                delete ptr_stack_frames;
                return;
            }
            stack_meta.p_stacktraces = ptr_stack_frames;
        }
    }
}

static inline void record_release_mem_unsafe(void *__ptr,
                                             std::map<void *, ptr_meta_t> &ptr_metas,
                                             std::map<void *, caller_meta_t> &caller_metas,
                                             std::map<uint64_t, stack_meta_t> &stack_metas) {
    if (!ptr_metas.count(__ptr)) { // 指针未记录
//        LOGD("Yves-debug", "skip for ptr not found");
        return;
    }

//    LOGD("Yves-debug", "record_release_mem_unsafe");

    ptr_meta_t &ptr_meta = ptr_metas.at(__ptr);

    if (ptr_meta.caller && caller_metas.count(ptr_meta.caller)) {
        caller_meta_t &caller_meta = caller_metas.at(ptr_meta.caller);
        if (caller_meta.total_size > ptr_meta.size) { // 减去 caller 的 size
            caller_meta.total_size -= ptr_meta.size;
            caller_meta.pointers.erase(__ptr);
        } else { // 删除 size 为 0 的 caller
            std::set<void *> empty_set;
            empty_set.swap(caller_meta.pointers);
//            caller_meta.pointers.clear(); // 造成卡顿, 是否有其他方法
            caller_metas.erase(ptr_meta.caller);
        }
    }

    if (is_stacktrace_enabled && ptr_meta.stack_hash) {
        stack_meta_t &stack_meta = stack_metas.at(ptr_meta.stack_hash);
        if (stack_meta.size > ptr_meta.size) { // 减去同堆栈的 size
            stack_meta.size -= ptr_meta.size;
        } else { // 删除 size 为 0 的堆栈
            delete stack_meta.p_stacktraces;
            stack_metas.erase(ptr_meta.stack_hash);
        }
    }

    ptr_metas.erase(__ptr);
}

static inline void on_acquire_memory(void *__caller, void *__ptr, size_t __byte_count) {
    acquire_lock();

    record_acquire_mem_unsafe(__caller, __ptr, __byte_count, m_ptr_meta, m_caller_meta,
                              m_stack_meta);

    release_lock();
}

static inline void on_release_memory(void *__ptr) {
    acquire_lock();

    record_release_mem_unsafe(__ptr, m_ptr_meta, m_caller_meta, m_stack_meta);

    release_lock();
}

static inline void on_mmap_memory(void *__caller, void *__ptr, size_t __byte_count) {
    acquire_lock();

    record_acquire_mem_unsafe(__caller, __ptr, __byte_count, m_mmap_ptr_meta, m_mmap_caller_meta,
                              m_mmap_stack_meta);

    release_lock();
}

static inline void on_munmap_memory(void *__ptr) {
    acquire_lock();

    record_release_mem_unsafe(__ptr, m_mmap_ptr_meta, m_mmap_caller_meta, m_mmap_stack_meta);

    release_lock();
}

static inline void dump_callers_unsafe(FILE *log_file,
                                       std::map<void *, ptr_meta_t> &ptr_metas,
                                       std::map<void *, caller_meta_t> &caller_metas) {
    if (caller_metas.empty()) {
        LOGE("Yves.dump", "caller: nothing dump");
        return;
    }

    LOGD("Yves.dump", "caller count = %zu", caller_metas.size());
    fprintf(log_file, "caller count = %zu\n", caller_metas.size());

    std::unordered_map<std::string, size_t> caller_alloc_size_of_so;
    std::unordered_map<std::string, std::map<size_t, size_t>> same_size_count_of_so;

//    LOGE("Yves.debug", "caller so begin");
    // 按 so 聚类
    for (auto i = caller_metas.begin(); i != caller_metas.end(); ++i) {
//        LOGE("Yves.debug", "so sum loop");
        auto caller = i->first;
        auto caller_meta = i->second;

        Dl_info dl_info;
        dladdr(caller, &dl_info);

        if (!dl_info.dli_fname) {
            continue;
        }
        caller_alloc_size_of_so[dl_info.dli_fname] += caller_meta.total_size;

        // 按 size 聚类
        for (auto p = caller_meta.pointers.begin(); p != caller_meta.pointers.end(); ++p) {
//            LOGE("Yves.debug", "size sum loop");
            if (ptr_metas.count(*p)) {
                auto ptr_meta = ptr_metas.at(*p);
                same_size_count_of_so[dl_info.dli_fname][ptr_meta.size]++;
            } else {
                // fixme why ?
//                LOGE("Yves.debug", "ptr_meta not found !!!");
            }
        }
    }

//    LOGE("Yves.debug", "begin sort");
    // 排序 so
    std::multimap<size_t, std::string> result_sort_by_size;

    std::transform(caller_alloc_size_of_so.begin(),
                   caller_alloc_size_of_so.end(),
                   std::inserter(result_sort_by_size, result_sort_by_size.begin()),
                   [](std::pair<std::string, size_t> src) {
                       return std::pair<size_t, std::string>(src.second, src.first);
                   });
//    LOGE("Yves.debug", "sort end");

    size_t caller_total_size = 0;
    for (auto i = result_sort_by_size.rbegin(); i != result_sort_by_size.rend(); ++i) {
        LOGD("Yves.dump", "so = %s, caller alloc size = %zu", i->second.c_str(), i->first);
        fprintf(log_file, "caller alloc size = %10zu b, so = %s\n", i->first, i->second.c_str());

        caller_total_size += i->first;

        auto count_of_size = same_size_count_of_so[i->second];
        std::multimap<size_t, std::pair<size_t, size_t>> result_sort_by_mul;
        std::transform(count_of_size.begin(),
                       count_of_size.end(),
                       std::inserter(result_sort_by_mul, result_sort_by_mul.begin()),
                       [](std::pair<size_t, size_t> src) {
                           return std::pair<size_t, std::pair<size_t, size_t >>(
                                   src.first * src.second,
                                   std::pair<size_t, size_t>(src.first, src.second));
                       });
        int lines = 20; // fixme hard coding
        LOGD("Yves.dump", "top %d (size * count):", lines);
        fprintf(log_file, "top %d (size * count):\n", lines);

        for (auto sc = result_sort_by_mul.rbegin();
             sc != result_sort_by_mul.rend() && lines; ++sc, --lines) {
            auto size = sc->second.first;
            auto count = sc->second.second;
            LOGD("Yves.dump", "   size = %10zu b, count = %zu", size, count);
            fprintf(log_file, "   size = %10zu b, count = %zu\n", size, count);
        }
    }

    LOGD("Yves.dump", "\n---------------------------------------------------");
    fprintf(log_file, "\n---------------------------------------------------\n");
    LOGD("Yves.dump", "| caller total size = %zu b", caller_total_size);
    fprintf(log_file, "| caller total size = %zu b\n", caller_total_size);
    LOGD("Yves.dump", "---------------------------------------------------\n");
    fprintf(log_file, "---------------------------------------------------\n\n");
}

static inline void dump_stacks_unsafe(FILE *log_file,
                                      std::map<uint64_t, stack_meta_t> &stack_metas) {
    if (stack_metas.empty()) {
        LOGE("Yves-dump", "stacktrace: nothing dump");
        return;
    }

    LOGD("Yves.dump", "hash count = %zu", stack_metas.size());
    fprintf(log_file, "hash count = %zu\n", stack_metas.size());

    std::unordered_map<std::string, size_t> stack_alloc_size_of_so;
    std::unordered_map<std::string, std::vector<std::pair<size_t, std::string>>> stacktrace_of_so;

    for (auto i = stack_metas.begin(); i != stack_metas.end(); ++i) {
        auto hash = i->first;
        auto size = i->second.size;
        auto stacktrace = i->second.p_stacktraces;

        std::string caller_so_name;
        std::stringstream stack_builder;
        for (auto it = stacktrace->begin(); it != stacktrace->end(); ++it) {
            Dl_info stack_info = {nullptr};
            int success = dladdr((void *) it->pc, &stack_info);

            std::string so_name = std::string(success ? stack_info.dli_fname : "");

            char *demangled_name = nullptr;
            if (success > 0) {
                int status = 0;
                demangled_name = abi::__cxa_demangle(stack_info.dli_sname, nullptr, 0, &status);
            }

            stack_builder << "      | "
                          << "#pc " << std::hex << it->rel_pc << " "
                          << (demangled_name ? demangled_name : "(null)")
                          << " (" << (success && stack_info.dli_fname ? stack_info.dli_fname : "(null)") << ")"
                          << std::endl;

            if (demangled_name) {
                free(demangled_name);
            }

            // fixme hard coding
            if (/*so_name.find("com.tencent.mm") == std::string::npos ||*/
                    so_name.find("libwxperf.so") != std::string::npos ||
                    !caller_so_name.empty()) {
                continue;
            }

            caller_so_name = so_name;
            stack_alloc_size_of_so[caller_so_name] += size;
        }

        std::pair<size_t, std::string> stack_size_pair(size, stack_builder.str());
        stacktrace_of_so[caller_so_name].push_back(stack_size_pair);
    }

    // 排序
    for (auto i = stack_alloc_size_of_so.begin(); i != stack_alloc_size_of_so.end(); ++i) {
        auto so_name = i->first;
        auto so_alloc_size = i->second;

        LOGD("Yves.dump", "\nmalloc size of so (%s) : remaining size = %zu", so_name.c_str(),
             so_alloc_size);
        fprintf(log_file, "\nmalloc size of so (%s) : remaining size = %zu\n", so_name.c_str(),
                so_alloc_size);

        for (auto it_stack = stacktrace_of_so[so_name].begin();
             it_stack != stacktrace_of_so[so_name].end(); ++it_stack) {

            LOGD("Yves.dump", "malloc size of the same stack = %zu\n stacktrace : \n%s",
                 it_stack->first,
                 it_stack->second.c_str());

            fprintf(log_file, "malloc size of the same stack = %zu\n stacktrace : \n%s\n",
                    it_stack->first,
                    it_stack->second.c_str());
        }
    }

}

static inline void dump_unsafe(FILE *log_file, bool enable_mmap_hook) {

    // native heap allocation
    dump_callers_unsafe(log_file, m_ptr_meta, m_caller_meta);
    dump_stacks_unsafe(log_file, m_stack_meta);

    fprintf(log_file,
            "<void *, ptr_meta_t> m_ptr_meta [%zu * %zu = (%zu)]\n<void *, caller_meta_t> m_caller_meta [%zu * %zu = (%zu)]\n<uint64_t, stack_meta_t> m_stack_meta [%zu * %zu = (%zu)]\n\n",
            sizeof(ptr_meta_t) + sizeof(void *), m_ptr_meta.size(),
            (sizeof(ptr_meta_t) + sizeof(void *)) * m_ptr_meta.size(),
            sizeof(caller_meta_t) + sizeof(void *), m_caller_meta.size(),
            (sizeof(caller_meta_t) + sizeof(void *)) * m_caller_meta.size(),
            sizeof(stack_meta_t) + sizeof(uint64_t), m_stack_meta.size(),
            (sizeof(stack_meta_t) + sizeof(uint64_t)) * m_stack_meta.size());

    if (!enable_mmap_hook) {
        return;
    }

    // mmap allocation
    LOGD("Yves.dump", "############################# mmap ###################################\n\n");
    fprintf(log_file, "############################# mmap ###################################\n\n");

    dump_callers_unsafe(log_file, m_mmap_ptr_meta, m_mmap_caller_meta);
    dump_stacks_unsafe(log_file, m_mmap_stack_meta);

    fprintf(log_file,
            "<void *, ptr_meta_t> m_mmap_ptr_meta [%zu * %zu = (%zu)]\n<void *, caller_meta_t> m_mmap_caller_meta [%zu * %zu = (%zu)]\n<uint64_t, stack_meta_t> m_mmap_stack_meta [%zu * %zu = (%zu)]\n\n",
            sizeof(ptr_meta_t) + sizeof(void *), m_mmap_ptr_meta.size(),
            (sizeof(ptr_meta_t) + sizeof(void *)) * m_mmap_ptr_meta.size(),
            sizeof(caller_meta_t) + sizeof(void *), m_mmap_caller_meta.size(),
            (sizeof(caller_meta_t) + sizeof(void *)) * m_mmap_caller_meta.size(),
            sizeof(stack_meta_t) + sizeof(uint64_t), m_mmap_stack_meta.size(),
            (sizeof(stack_meta_t) + sizeof(uint64_t)) * m_mmap_stack_meta.size());
}

void dump(bool enable_mmap_hook, const char *path) {
    LOGD("Yves.dump",
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> memory dump begin <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");

    acquire_lock();

    FILE *log_file = fopen(path, "w+");
    LOGD("Yves.dump", "dump path = %s", path);
    if (!log_file) {
        LOGE("Yves.dump", "open file failed");
        release_lock();
        return;
    }

    dump_unsafe(log_file, enable_mmap_hook);

    fflush(log_file);
    fclose(log_file);

    release_lock();
    LOGD("Yves.dump",
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> memory dump end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
}

void memory_hook_on_dlopen(const char *__file_name) {
    LOGD("Yves-debug", "memory_hook_on_dlopen");
    if (is_stacktrace_enabled) {
        acquire_lock();
        unwindstack::update_maps();
        release_lock();
    }
    srand((unsigned int) time(NULL));
}

DEFINE_HOOK_FUN(void *, malloc, size_t __byte_count) {
    CALL_ORIGIN_FUNC_RET(void*, p, malloc, __byte_count);
    DO_HOOK_ACQUIRE(p, __byte_count);
    return p;
}

DEFINE_HOOK_FUN(void *, calloc, size_t __item_count, size_t __item_size) {
    CALL_ORIGIN_FUNC_RET(void *, p, calloc, __item_count, __item_size);
    DO_HOOK_ACQUIRE(p, __item_count * __item_size);
    return p;
}

DEFINE_HOOK_FUN(void *, realloc, void *__ptr, size_t __byte_count) {
    CALL_ORIGIN_FUNC_RET(void *, p, realloc, __ptr, __byte_count);

    GET_CALLER_ADDR(caller);

    // If ptr is NULL, then the call is equivalent to malloc(size), for all values of size;
    // if size is equal to zero, and ptr is not NULL, then the call is equivalent to free(ptr).
    // Unless ptr is NULL, it must have been returned by an earlier call to malloc(), calloc() or realloc().
    // If the area pointed to was moved, a free(ptr) is done.
    if (!__ptr) { // malloc
        on_acquire_memory(caller, p, __byte_count);
        return p;
    } else if (!__byte_count) { // free
        on_release_memory(__ptr);
        return p;
    }

    // whatever has been moved or not, record anyway, because using realloc to shrink an allocation is allowed.
    on_release_memory(__ptr);
    on_acquire_memory(caller, p, __byte_count);

    return p;
}

DEFINE_HOOK_FUN(void, free, void *__ptr) {
    DO_HOOK_RELEASE(__ptr);
    CALL_ORIGIN_FUNC_VOID(free, __ptr);
}

#if defined(__USE_FILE_OFFSET64)
void*h_mmap(void* __addr, size_t __size, int __prot, int __flags, int __fd, off_t __offset) __RENAME(mmap64) {
    void * p = mmap(__addr, __size, __prot, __flags, __fd, __offset);
    GET_CALLER_ADDR(caller);
    on_mmap_memory(caller, p, __size);
    return p;
}
#else

DEFINE_HOOK_FUN(void *, mmap, void *__addr, size_t __size, int __prot, int __flags, int __fd,
                off_t __offset) {
    CALL_ORIGIN_FUNC_RET(void *, p, mmap, __addr, __size, __prot, __flags, __fd, __offset);
    if (p == MAP_FAILED) {
        return p;// just return
    }
    DO_HOOK_ACQUIRE(p, __size);
    return p;
}


#endif

#if __ANDROID_API__ >= __ANDROID_API_L__

void *h_mmap64(void *__addr, size_t __size, int __prot, int __flags, int __fd,
               off64_t __offset) __INTRODUCED_IN(21) {
    void *p = mmap64(__addr, __size, __prot, __flags, __fd, __offset);
    if (p == MAP_FAILED) {
        return p;// just return
    }
    GET_CALLER_ADDR(caller);
    on_mmap_memory(caller, p, __size);
    return p;
}

#endif

DEFINE_HOOK_FUN(void *, mremap, void *__old_addr, size_t __old_size, size_t __new_size, int __flags,
                ...) {
    void *new_address = nullptr;
    if ((__flags & MREMAP_FIXED) != 0) {
        va_list ap;
        va_start(ap, __flags);
        new_address = va_arg(ap, void *);
        va_end(ap);
    }
    void *p = mremap(__old_addr, __old_size, __new_size, __flags, new_address);
    if (p == MAP_FAILED) {
        return p; // just return
    }

    GET_CALLER_ADDR(caller);

    on_munmap_memory(__old_addr);
    on_mmap_memory(caller, p, __new_size);

    return p;
}

DEFINE_HOOK_FUN(int, munmap, void *__addr, size_t __size) {
    on_munmap_memory(__addr);
    return munmap(__addr, __size);
}

#ifndef __LP64__

DEFINE_HOOK_FUN(void*, _Znwj, size_t size) {
//    void * p = ORIGINAL_FUNC_NAME(_Znwj)(size);
    CALL_ORIGIN_FUNC_RET(void*, p, _Znwj, size);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnwjSt11align_val_t, size_t size, std::align_val_t align_val) {
//    void * p = ORIGINAL_FUNC_NAME(_ZnwjSt11align_val_t)(size, align_val);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnwjSt11align_val_t, size, align_val);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnwjSt11align_val_tRKSt9nothrow_t, size_t size,
                    std::align_val_t align_val, std::nothrow_t const& nothrow) {
//    void * p = ORIGINAL_FUNC_NAME(_ZnwjSt11align_val_tRKSt9nothrow_t)(size, align_val, nothrow);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnwjSt11align_val_tRKSt9nothrow_t, size, align_val, nothrow);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnwjRKSt9nothrow_t, size_t size, std::nothrow_t const& nothrow) {
//    void * p = ORIGINAL_FUNC_NAME(_ZnwjRKSt9nothrow_t)(size, nothrow);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnwjRKSt9nothrow_t, size, nothrow);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _Znaj, size_t size) {
//    void * p = ORIGINAL_FUNC_NAME(_Znaj)(size);
    CALL_ORIGIN_FUNC_RET(void*, p, _Znaj, size);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnajSt11align_val_t, size_t size, std::align_val_t align_val) {
//    void * p = ORIGINAL_FUNC_NAME(_ZnajSt11align_val_t)(size, align_val);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnajSt11align_val_t, size, align_val);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnajSt11align_val_tRKSt9nothrow_t, size_t size,
                    std::align_val_t align_val, std::nothrow_t const& nothrow) {
//    void * p = ORIGINAL_FUNC_NAME(_ZnajSt11align_val_tRKSt9nothrow_t)(size, align_val, nothrow);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnajSt11align_val_tRKSt9nothrow_t, size, align_val, nothrow);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnajRKSt9nothrow_t, size_t size, std::nothrow_t const& nothrow) {
//    void * p = ORIGINAL_FUNC_NAME(_ZnajRKSt9nothrow_t)(size, nothrow);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnajRKSt9nothrow_t, size, nothrow);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void, _ZdaPvj, void* ptr, size_t size) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdaPvj)(ptr, size);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvj, ptr, size);
}

DEFINE_HOOK_FUN(void, _ZdaPvjSt11align_val_t, void* ptr, size_t size,
                    std::align_val_t align_val) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdaPvjSt11align_val_t)(ptr, size, align_val);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvjSt11align_val_t, ptr, size, align_val);
}

DEFINE_HOOK_FUN(void, _ZdlPvj, void* ptr, size_t size) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdlPvj)(ptr, size);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvj, ptr, size);
}

DEFINE_HOOK_FUN(void, _ZdlPvjSt11align_val_t, void* ptr, size_t size,
                    std::align_val_t align_val) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdlPvjSt11align_val_t)(ptr, size, align_val);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvjSt11align_val_t, ptr, size, align_val);
}

#else

DEFINE_HOOK_FUN(void*, _Znwm, size_t size) {
    CALL_ORIGIN_FUNC_RET(void*, p, _Znwm, size);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnwmSt11align_val_t, size_t size, std::align_val_t align_val) {
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnwmSt11align_val_t, size, align_val);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnwmSt11align_val_tRKSt9nothrow_t, size_t size,
                std::align_val_t align_val, std::nothrow_t const &nothrow) {
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnwmSt11align_val_tRKSt9nothrow_t, size, align_val, nothrow);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnwmRKSt9nothrow_t, size_t size, std::nothrow_t const &nothrow) {
    CALL_ORIGIN_FUNC_RET(void*, p, _Znwm, size);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _Znam, size_t size) {
    CALL_ORIGIN_FUNC_RET(void*, p, _Znam, size);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnamSt11align_val_t, size_t size, std::align_val_t align_val) {
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnamSt11align_val_t, size, align_val);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnamSt11align_val_tRKSt9nothrow_t, size_t size,
                std::align_val_t align_val, std::nothrow_t const &nothrow) {
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnamSt11align_val_tRKSt9nothrow_t, size, align_val, nothrow);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnamRKSt9nothrow_t, size_t size, std::nothrow_t const &nothrow) {
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnamRKSt9nothrow_t, size, nothrow);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void, _ZdlPvm, void *ptr, size_t size) {
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvm, ptr, size);
}

DEFINE_HOOK_FUN(void, _ZdlPvmSt11align_val_t, void *ptr, size_t size,
                std::align_val_t align_val) {
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvmSt11align_val_t, ptr, size, align_val);
}

DEFINE_HOOK_FUN(void, _ZdaPvm, void *ptr, size_t size) {
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvm, ptr, size);
}

DEFINE_HOOK_FUN(void, _ZdaPvmSt11align_val_t, void *ptr, size_t size,
                std::align_val_t align_val) {
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvmSt11align_val_t, ptr, size, align_val);
}

#endif

DEFINE_HOOK_FUN(char*, strdup, const char *str) {
    CALL_ORIGIN_FUNC_RET(char *, p, strdup, str);
    return p;
}

DEFINE_HOOK_FUN(char*, strndup, const char *str, size_t n) {
    CALL_ORIGIN_FUNC_RET(char *, p, strndup, str, n);
    return p;
}

DEFINE_HOOK_FUN(void, _ZdlPv, void *p) {
    DO_HOOK_RELEASE(p);
    CALL_ORIGIN_FUNC_VOID(_ZdlPv, p);
}

DEFINE_HOOK_FUN(void, _ZdlPvSt11align_val_t, void *ptr, std::align_val_t align_val) {
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvSt11align_val_t, ptr, align_val);
}

DEFINE_HOOK_FUN(void, _ZdlPvSt11align_val_tRKSt9nothrow_t, void *ptr,
                std::align_val_t align_val, std::nothrow_t const &nothrow) {
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvSt11align_val_tRKSt9nothrow_t, ptr, align_val, nothrow);
}

DEFINE_HOOK_FUN(void, _ZdlPvRKSt9nothrow_t, void *ptr, std::nothrow_t const &nothrow) {
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvRKSt9nothrow_t, ptr, nothrow);
}

DEFINE_HOOK_FUN(void, _ZdaPv, void *ptr) {
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdaPv, ptr);
}

DEFINE_HOOK_FUN(void, _ZdaPvSt11align_val_t, void *ptr, std::align_val_t align_val) {
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvSt11align_val_t, ptr, align_val);
}

DEFINE_HOOK_FUN(void, _ZdaPvSt11align_val_tRKSt9nothrow_t, void *ptr,
                std::align_val_t align_val, std::nothrow_t const &nothrow) {
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvSt11align_val_tRKSt9nothrow_t, ptr, align_val, nothrow);
}

DEFINE_HOOK_FUN(void, _ZdaPvRKSt9nothrow_t, void *ptr, std::nothrow_t const &nothrow) {
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvRKSt9nothrow_t, ptr, nothrow);
}

#undef ORIGINAL_LIB