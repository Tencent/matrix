//
// Created by Yves on 2019-08-08.
// TODO 代码整理, 逻辑拆分
// TODO 大批量分配后大批量删除 case 的性能
// notice: 加锁顺序, 谨防死锁
//

#include <malloc.h>
#include <unistd.h>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <list>
#include <map>
#include <random>
#include <xhook.h>
#include <sstream>
#include <cxxabi.h>
#include <sys/mman.h>
#include <mutex>
#include <condition_variable>
#include <shared_mutex>
#include "MemoryHookFunctions.h"
#include "StackTrace.h"
#include "Utils.h"
#include "unwindstack/Unwinder.h"
#include "ThreadPool.h"
#include "MemoryHookMetas.h"
#include "MemoryHook.h"

static pointer_meta_container           m_ptr_meta_container;
static std::map<uint64_t, stack_meta_t> m_stack_metas; // fixme lock
static std::mutex                       m_stack_meta_mutex;

static bool is_stacktrace_enabled      = false;
static bool is_caller_sampling_enabled = false;

static size_t m_sample_size_min = 0;
static size_t m_sample_size_max = 0;
static double m_sampling        = 0.01;

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

void enable_caller_sampling(bool __enable) {
    is_caller_sampling_enabled = __enable;
}

static inline void
decrease_stack_size(std::map<uint64_t, stack_meta_t> &stack_metas,
                    const ptr_meta_t &__meta) {
    LOGI(TAG, "calculate_stack_size");

}

void memory_hook_init() {
    LOGI(TAG, "memory_hook_init");
}

static inline bool should_do_unwind(size_t __byte_count, void *__caller) {
    if (!is_caller_sampling_enabled) {

        return ((m_sample_size_min == 0 || __byte_count >= m_sample_size_min)
                && (m_sample_size_max == 0 || __byte_count <= m_sample_size_max)
                && rand() <= m_sampling * RAND_MAX);

    }

    // TODO caller sampling
    return false;
}

static inline void on_acquire_memory(void *__caller,
                                     void *__ptr,
                                     size_t __byte_count,
                                     bool __is_mmap) {
//    NanoSeconds_Start(alloc_begin);
////    NanoSeconds_End(alloc_cost, alloc_begin);
////    LOGD(TAG, "alloc stamp cost %lld", alloc_cost);

    if (!__ptr) {
        LOGE(TAG, "on_acquire_memory: invalid pointer");
        return;
    }

    ptr_meta_t &meta = m_ptr_meta_container[__ptr];

    meta.ptr        = __ptr;
    meta.size       = __byte_count;
    meta.caller     = __caller;
    meta.stack_hash = 0;
    meta.is_mmap    = __is_mmap;
//    meta.recycled   = false;

    // TODO 获取 Java 堆栈
    if (is_stacktrace_enabled && should_do_unwind(meta.size, meta.caller)) {

        std::vector<unwindstack::FrameData> stack_frames;
        unwind_adapter(stack_frames);

        if (!stack_frames.empty()) {
            uint64_t stack_hash = hash_stack_frames(stack_frames);
            meta.stack_hash = stack_hash;

            std::lock_guard<std::mutex> stack_meta_lock(m_stack_meta_mutex);

            stack_meta_t &stack_meta = m_stack_metas[stack_hash];
            stack_meta.size += meta.size;

            if (stack_meta.stacktrace.empty()) { // 相同的堆栈只记录一个
                stack_meta.stacktrace.swap(stack_frames);
            }
        }
    }

//    NanoSeconds_End(alloc_cost, alloc_begin);
//    LOGD(TAG, "alloc cost %lld", alloc_cost);
}

static inline void on_release_memory(void *__ptr, bool __is_mmap) {
    if (!__ptr) {
        LOGE(TAG, "on_release_memory: invalid pointer");
        return;
    }
//    NanoSeconds_Start(release_begin);

    // avoid copy
    m_ptr_meta_container.erase(__ptr, [](ptr_meta_t &__meta) {
        // notice: container is locked here
        if (is_stacktrace_enabled && __meta.stack_hash) {

            std::lock_guard<std::mutex> stack_meta_lock(m_stack_meta_mutex);

            if (m_stack_metas.count(__meta.stack_hash)) {
                stack_meta_t &stack_meta = m_stack_metas.at(__meta.stack_hash);

                if (stack_meta.size > __meta.size) { // 减去同堆栈的 size
                    stack_meta.size -= __meta.size;
                } else { // 删除 size 为 0 的堆栈
                    m_stack_metas.erase(__meta.stack_hash);
                }
            }
        }
    });
//    NanoSeconds_End(release_cost, release_begin);
//    LOGD(TAG, "release cost %lld", release_cost);
}

void on_alloc_memory(void *__caller, void *__ptr, size_t __byte_count) {
    on_acquire_memory(__caller, __ptr, __byte_count, false);
}

void on_free_memory(void *__ptr) {
    on_release_memory(__ptr, true);
}

void on_mmap_memory(void *__caller, void *__ptr, size_t __byte_count) {
    on_acquire_memory(__caller, __ptr, __byte_count, true);
}

void on_munmap_memory(void *__ptr) {
    on_release_memory(__ptr, true);
}

/**
 * 区分 native heap 和 mmap 的 caller 和 stack
 * @param __merge_bucket
 * @param __heap_caller_metas
 * @param __mmap_caller_metas
 * @param __heap_stack_metas
 * @param __mmap_stack_metas
 *
 * fixme 去掉中转逻辑？
 */
static inline size_t collect_metas(std::map<void *, caller_meta_t> &__heap_caller_metas,
                                   std::map<void *, caller_meta_t> &__mmap_caller_metas,
                                   std::map<uint64_t, stack_meta_t> &__heap_stack_metas,
                                   std::map<uint64_t, stack_meta_t> &__mmap_stack_metas) {
    LOGD(TAG, "collect_metas");

    size_t ptr_meta_size = 0;

    m_ptr_meta_container.for_each([&](const void *ptr, ptr_meta_t meta) {
        auto &dest_caller_metes = meta.is_mmap ? __mmap_caller_metas : __heap_caller_metas;
        auto &dest_stack_metas  = meta.is_mmap ? __mmap_stack_metas : __heap_stack_metas;

        if (meta.caller) {
            caller_meta_t &caller_meta = dest_caller_metes[meta.caller];
            caller_meta.pointers.insert(ptr);
            caller_meta.total_size += meta.size;
        }

        if (meta.stack_hash) {
            std::lock_guard<std::mutex> stack_meta_lock(m_stack_meta_mutex);
            if (m_stack_metas.count(meta.stack_hash)) {
                auto &stack_meta = dest_stack_metas[meta.stack_hash];
                stack_meta.stacktrace = m_stack_metas.at(meta.stack_hash).stacktrace;
                stack_meta.size += meta.size;
            }
        }

        ptr_meta_size++;
    });

    LOGD(TAG, "collect_metas done");
    return ptr_meta_size;
}

static inline void dump_callers(FILE *log_file,
//                                const std::multimap<void *, ptr_meta_t> &ptr_metas,
                                std::map<void *, caller_meta_t> &caller_metas) {

    if (caller_metas.empty()) {
        LOGI(TAG, "dump_callers: nothing dump");
        return;
    }

    LOGD(TAG, "dump_callers: count = %zu", caller_metas.size());
    fprintf(log_file, "dump_callers: count = %zu\n", caller_metas.size());

    std::unordered_map<std::string, size_t>                   caller_alloc_size_of_so;
    std::unordered_map<std::string, std::map<size_t, size_t>> same_size_count_of_so;

    LOGD(TAG, "caller so begin");
    // 按 so 聚类
    for (auto &i : caller_metas) {
        auto caller      = i.first;
        auto caller_meta = i.second;

        Dl_info dl_info;
        dladdr(caller, &dl_info);

        if (!dl_info.dli_fname) {
            continue;
        }
        caller_alloc_size_of_so[dl_info.dli_fname] += caller_meta.total_size;

        // 按 size 聚类
        for (auto pointer : caller_meta.pointers) {
            m_ptr_meta_container.get(pointer,
                                     [&same_size_count_of_so, &dl_info](ptr_meta_t &__meta) {
                                         same_size_count_of_so[dl_info.dli_fname][__meta.size]++;
                                     });
        }
    }

    // 排序 so
    std::multimap<size_t, std::string> result_sort_by_size;

    std::transform(caller_alloc_size_of_so.begin(),
                   caller_alloc_size_of_so.end(),
                   std::inserter(result_sort_by_size, result_sort_by_size.begin()),
                   [](const std::pair<std::string, size_t> &src) {
                       return std::pair<size_t, std::string>(src.second, src.first);
                   });

    size_t    caller_total_size = 0;
    for (auto i                 = result_sort_by_size.rbegin();
         i != result_sort_by_size.rend(); ++i) {
        LOGD(TAG, "so = %s, caller alloc size = %zu", i->second.c_str(), i->first);
        fprintf(log_file, "caller alloc size = %10zu b, so = %s\n", i->first, i->second.c_str());

        caller_total_size += i->first;

        auto                                             count_of_size = same_size_count_of_so[i->second];
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
        LOGD(TAG, "top %d (size * count):", lines);
        fprintf(log_file, "top %d (size * count):\n", lines);

        for (auto sc = result_sort_by_mul.rbegin();
             sc != result_sort_by_mul.rend() && lines; ++sc, --lines) {
            auto size  = sc->second.first;
            auto count = sc->second.second;
            LOGD(TAG, "   size = %10zu b, count = %zu", size, count);
            fprintf(log_file, "   size = %10zu b, count = %zu\n", size, count);
        }
    }

    LOGD(TAG, "\n---------------------------------------------------");
    fprintf(log_file, "\n---------------------------------------------------\n");
    LOGD(TAG, "| caller total size = %zu b", caller_total_size);
    fprintf(log_file, "| caller total size = %zu b\n", caller_total_size);
    LOGD(TAG, "---------------------------------------------------\n");
    fprintf(log_file, "---------------------------------------------------\n\n");
}

//static inline void dump_debug_stacks() {
//    for (auto map_it : m_debug_lost_ptr_stack) {
//        auto ptr        = map_it.first;
//        auto stack_meta = map_it.second;
//
//        LOGD(TAG, "LOST ptr %p, stack = ", ptr);
//        for (auto &it : *stack_meta.p_stacktraces) {
//            Dl_info stack_info = {nullptr};
//            int     success    = dladdr((void *) it.pc, &stack_info);
//
//            std::stringstream stack_builder;
//
//            char *demangled_name = nullptr;
//            if (success > 0) {
//                int status = 0;
//                demangled_name = abi::__cxa_demangle(stack_info.dli_sname, nullptr, 0, &status);
//            }
//
//            stack_builder << "      | "
//                          << "#pc " << std::hex << it.rel_pc << " "
//                          << (demangled_name ? demangled_name : "(null)")
//                          << " ("
//                          << (success && stack_info.dli_fname ? stack_info.dli_fname : "(null)")
//                          << ")"
//                          << std::endl;
//
//            LOGD(TAG, "%s", stack_builder.str().c_str());
//
//            if (demangled_name) {
//                free(demangled_name);
//            }
//        }
//    }
//
//    m_debug_lost_ptr_stack.clear();
//}

static inline void dump_stacks(FILE *log_file,
                               std::map<uint64_t, stack_meta_t> &stack_metas) {
    if (stack_metas.empty()) {
        LOGI(TAG, "stacktrace: nothing dump");
        return;
    }

    LOGD(TAG, "dump_stacks: hash count = %zu", stack_metas.size());
    fprintf(log_file, "dump_stacks: hash count = %zu\n", stack_metas.size());

    for (auto &stack_meta :stack_metas) {
        LOGD(TAG, "hash %lu : stack.size = %zu", stack_meta.first, stack_meta.second.size);
    }

    std::unordered_map<std::string, size_t>                                      stack_alloc_size_of_so;
    std::unordered_map<std::string, std::vector<std::pair<size_t, std::string>>> stacktrace_of_so;

    for (auto &stack_meta : stack_metas) {
        auto hash            = stack_meta.first;
        auto size            = stack_meta.second.size;
        auto stacktrace      = stack_meta.second.stacktrace;

        std::string       caller_so_name;
        std::stringstream stack_builder;

        restore_frame_data(stacktrace);

        for (auto & it : stacktrace) {

            std::string so_name = it.map_name;

            char *demangled_name = nullptr;
            int status = 0;
            demangled_name = abi::__cxa_demangle(it.function_name.c_str(), nullptr, 0, &status);

            stack_builder << "      | "
                          << "#pc " << std::hex << it.rel_pc << " "
                          << (demangled_name ? demangled_name : "(null)")
                          << " ("
                          << it.map_name
                          << ")"
                          << std::endl;

            LOGE(TAG, "#pc %p %s %s", (void *) it.rel_pc, demangled_name, it.map_name.c_str());

            if (demangled_name) {
                free(demangled_name);
            }

            // fixme hard coding
            if (/*so_name.find("com.tencent.mm") == std::string::npos ||*/
                    so_name.find("libwxperf.so") != std::string::npos ||
                    so_name.find("libwxperf-jni.so") != std::string::npos ||
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
        auto so_name       = i->first;
        auto so_alloc_size = i->second;

        LOGD(TAG, "\nmalloc size of so (%s) : remaining size = %zu", so_name.c_str(),
             so_alloc_size);
        fprintf(log_file, "\nmalloc size of so (%s) : remaining size = %zu\n", so_name.c_str(),
                so_alloc_size);

        for (auto it_stack = stacktrace_of_so[so_name].begin();
             it_stack != stacktrace_of_so[so_name].end(); ++it_stack) {

            LOGD(TAG, "malloc size of the same stack = %zu\n stacktrace : \n%s",
                 it_stack->first,
                 it_stack->second.c_str());

            fprintf(log_file, "malloc size of the same stack = %zu\n stacktrace : \n%s\n",
                    it_stack->first,
                    it_stack->second.c_str());
        }
    }

}

static inline void dump_impl(FILE *log_file, bool enable_mmap_hook) {

    std::map<void *, ptr_meta_t>     ptr_metas;
    std::map<void *, caller_meta_t>  heap_caller_metas;
    std::map<void *, caller_meta_t>  mmap_caller_metas;
    std::map<uint64_t, stack_meta_t> heap_stack_metas;
    std::map<uint64_t, stack_meta_t> mmap_stack_metas;

    size_t ptr_meta_size = collect_metas(heap_caller_metas,
                                         mmap_caller_metas,
                                         heap_stack_metas,
                                         mmap_stack_metas);

    // native heap allocation
    dump_callers(log_file, heap_caller_metas);
    dump_stacks(log_file, heap_stack_metas);

    if (enable_mmap_hook) {
        // mmap allocation
        LOGD(TAG, "############################# mmap ###################################\n\n");
        fprintf(log_file,
                "############################# mmap ###################################\n\n");

        dump_callers(log_file, mmap_caller_metas);
        dump_stacks(log_file, mmap_stack_metas);
    }

    std::lock_guard<std::mutex> stack_meta_lock(m_stack_meta_mutex);

    fprintf(log_file,
            "<void *, ptr_meta_t> ptr_meta [%zu * %zu = (%zu)]\n"
            "<uint64_t, stack_meta_t> stack_meta [%zu * %zu = (%zu)]\n",

            sizeof(ptr_meta_t) + sizeof(void *), ptr_meta_size,
            (sizeof(ptr_meta_t) + sizeof(void *)) * ptr_meta_size,

            sizeof(stack_meta_t) + sizeof(uint64_t), m_stack_metas.size(),
            (sizeof(stack_meta_t) + sizeof(uint64_t)) * m_stack_metas.size());

    LOGD(TAG,
         "<void *, ptr_meta_t> ptr_meta [%zu * %zu = (%zu)]\n"
         "<uint64_t, stack_meta_t> stack_meta [%zu * %zu = (%zu)]\n",

         sizeof(ptr_meta_t) + sizeof(void *), ptr_meta_size,
         (sizeof(ptr_meta_t) + sizeof(void *)) * ptr_meta_size,

         sizeof(stack_meta_t) + sizeof(uint64_t), m_stack_metas.size(),
         (sizeof(stack_meta_t) + sizeof(uint64_t)) * m_stack_metas.size());
}

void dump(bool enable_mmap_hook, const char *path) {
    LOGD(TAG,
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> memory dump begin <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");

    assert(path != nullptr);
    FILE *log_file = fopen(path, "w+");
    LOGD(TAG, "dump path = %s", path);
    if (!log_file) {
        LOGE(TAG, "open file failed");
        return;
    }

    dump_impl(log_file, enable_mmap_hook);

    fflush(log_file);
    fclose(log_file);

    LOGD(TAG,
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> memory dump end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
}

extern const HookFunction HOOK_MALL_FUNCTIONS[];

void memory_hook_on_dlopen(const char *__file_name) {
    LOGD(TAG, "memory_hook_on_dlopen: file %s, h_malloc %p, h_realloc %p, h_free %p", __file_name,
         h_malloc, h_realloc, h_free);
    if (is_stacktrace_enabled) {
        notify_maps_change();
    }
    srand((unsigned int) time(NULL));
}

