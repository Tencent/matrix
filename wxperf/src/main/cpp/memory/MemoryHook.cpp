//
// Created by Yves on 2019-08-08.
//

#include <malloc.h>
#include <dlfcn.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <xhook.h>
#include <sstream>
#include <cxxabi.h>
#include "MemoryHook.h"
#include "lock.h"
#include "StackTrace.h"
#include "utils.h"
#include "unwindstack/Unwinder.h"

extern "C" typedef struct {
    size_t size;
    void *caller;
    uint64_t stack_hash;
} ptr_meta_t;

extern "C" typedef struct {
    size_t total_size;
    std::unordered_set<void *> pointers;
} caller_meta_t;

extern "C" typedef struct {
    size_t size;
    std::vector<unwindstack::FrameData> *p_stacktraces;
} stack_meta_t;

std::unordered_map<void *, ptr_meta_t> m_ptr_meta;
std::unordered_map<void *, caller_meta_t> m_caller_meta;
std::unordered_map<uint64_t, stack_meta_t> m_stack_meta;

//std::unordered_map<void *, size_t> *m_size_of_caller;
//std::unordered_map<void *, size_t> *m_size_of_pointer;
//std::unordered_map<void *, void *> *m_caller_of_pointer;
//std::unordered_map<void *, std::unordered_set<void *>> *m_pointers_of_caller;
//
//std::unordered_map<void *, uint64_t> *m_pc_hash_of_pointer;
//std::unordered_map<uint64_t, size_t> *m_size_of_hash;
//std::unordered_map<uint64_t, std::vector<unwindstack::FrameData> *> *m_stacktrace_of_hash;

bool is_stacktrace_enabled = false;
bool is_group_by_size_enabled = false;

size_t m_sample_size_min = 0;
size_t m_sample_size_max = 0;

double m_sampling = 1;

void enableStacktrace(bool __enable) {
    is_stacktrace_enabled = __enable;
}

void enableGroupBySize(bool __enable) {
    is_group_by_size_enabled = __enable;
}

void setSampleSizeRange(size_t __min, size_t __max) {
    m_sample_size_min = __min;
    m_sample_size_max = __max;
}

void setSampling(double __sampling) {
    m_sampling = __sampling;
}

//static inline void init_if_necessary() {
//    if (!m_size_of_caller) {
//        LOGD("Yves", "init size of caller");
//        m_size_of_caller = new std::unordered_map<void *, size_t>;
//    }
//
//    if (!m_size_of_pointer) {
//        m_size_of_pointer = new std::unordered_map<void *, size_t>;
//    }
//
//    if (!m_caller_of_pointer) {
//        m_caller_of_pointer = new std::unordered_map<void *, void *>;
//    }
//
//    if (!m_pc_hash_of_pointer) {
//        m_pc_hash_of_pointer = new std::unordered_map<void *, uint64_t>;
//    }
////
////    if (!m_size_of_so) {
////        m_size_of_so = new std::unordered_map<std::string, size_t>;
////    }
//
//    if (!m_size_of_hash) {
//        m_size_of_hash = new std::unordered_map<uint64_t, size_t>;
//    }
//
//    if (!m_stacktrace_of_hash) {
//        m_stacktrace_of_hash = new std::unordered_map<uint64_t, std::vector<unwindstack::FrameData> *>;
//    }
//
//    if (!m_pointers_of_caller) {
//        m_pointers_of_caller = new std::unordered_map<void *, std::unordered_set<void *>>;
//    }
//}

//static inline void on_acquire_memory_old(void *__caller, void *__ptr, size_t __byte_count) {
//    acquire_lock();
//    init_if_necessary();
////    LOGD("Yves", "+++++++++on acquire memory, ptr = (%p), size = (%zu)", __ptr, __byte_count);
//
//    if (m_size_of_pointer->count(__ptr) && m_size_of_pointer->at(__ptr) == __byte_count) {
//        LOGE("Yves", "!!!!!!!! redundant pointer and size!!!!!!!");
//        release_lock();
//        return;
//    }
//
//    (*m_size_of_caller)[__caller] += __byte_count;
//    (*m_size_of_pointer)[__ptr] = __byte_count;
//    (*m_caller_of_pointer)[__ptr] = __caller;
//
//    if (is_group_by_size_enabled) {
//        (*m_pointers_of_caller)[__caller].insert(__ptr);
//    }
//
//
//    if (is_stacktrace_enabled &&
//        (m_sample_size_min == 0 || __byte_count >= m_sample_size_min) &&
//        (m_sample_size_max == 0 || __byte_count <= m_sample_size_max)) {
//
//        int r = rand();
//
//        if (r > m_sampling * RAND_MAX) {
//            release_lock();
//            return;
//        }
//
//        LOGD("Yves", "do unwind");
//        auto stack_frames = new std::vector<unwindstack::FrameData>;
//        unwindstack::do_unwind(*stack_frames);
//
//        if (!stack_frames->empty()) {
//            uint64_t stack_hash = hash((*stack_frames));
//
//            (*m_pc_hash_of_pointer)[__ptr] = stack_hash;
//            (*m_stacktrace_of_hash)[stack_hash] = stack_frames;
//            (*m_size_of_hash)[stack_hash] += __byte_count;
//        }
//    }
//    release_lock();
//}

//static inline void on_release_memory_old(void *__caller, void *__ptr) {
//    acquire_lock();
//    init_if_necessary();
////    LOGD("Yves", "---------on release memory, ptr = (%p)", __ptr);
//    // with caller
//
//    if (!m_size_of_pointer->count(__ptr)) {
////        LOGD("Yves.ERROR", "can NOT find size of given pointer(%p), caller(%p)",
////                __ptr, m_caller_of_pointer->count(__ptr) ? (void *)m_size_of_caller->at(__ptr) : 0);
//        release_lock();
//        return;
//    }
//
//    auto ptr_size = m_size_of_pointer->at(__ptr);
//    m_size_of_pointer->erase(__ptr);
//    if (m_caller_of_pointer->count(__ptr)) {
//        auto alloc_caller = m_caller_of_pointer->at(__ptr);
//        m_caller_of_pointer->erase(__ptr);
//
//        if (m_size_of_caller->count(alloc_caller)) {
//            auto caller_size = m_size_of_caller->at(alloc_caller);
//
//            if (caller_size > ptr_size) {
//                (*m_size_of_caller)[alloc_caller] = caller_size - ptr_size;
//            } else {
//                m_size_of_caller->erase(alloc_caller);
//                if (is_group_by_size_enabled) {
//                    m_pointers_of_caller->erase(alloc_caller);
//                }
//            }
//        }
//
//        if (is_group_by_size_enabled) {
//            (*m_pointers_of_caller)[alloc_caller].erase(__ptr);
//        }
//    }
//
//    // with stacktrace
//    if (is_stacktrace_enabled && m_pc_hash_of_pointer->count(__ptr)) {
//        auto pc_hash = m_pc_hash_of_pointer->at(__ptr);
//        if (m_size_of_hash->count(pc_hash)) {
//            auto hash_size = m_size_of_hash->at(pc_hash);
//            if (hash_size > ptr_size) {
//                (*m_size_of_hash)[pc_hash] = hash_size - ptr_size;
//            } else {
//                m_size_of_hash->erase(pc_hash);
//            }
//        }
//    }
//    release_lock();
//}

static inline void on_acquire_memory(void *__caller, void *__ptr, size_t __byte_count) {
    acquire_lock();

    if (m_ptr_meta.count(__ptr) && m_ptr_meta.at(__ptr).size == __byte_count) { // 检查是否重复记录同一个指针
        LOGE("Yves", "!!!!!!!! redundant pointer and size!!!!!!!");
        release_lock();
        return;
    }
//    LOGD("Yves.debug", "on acquire ptr = %p", __ptr);
    ptr_meta_t& ptr_meta = m_ptr_meta[__ptr];
    ptr_meta.size = __byte_count;
    ptr_meta.caller = __caller;

    caller_meta_t& caller_meta = m_caller_meta[__caller];
    caller_meta.pointers.insert(__ptr);
    caller_meta.total_size += __byte_count;

    if (is_stacktrace_enabled &&
        (m_sample_size_min == 0 || __byte_count >= m_sample_size_min) &&
        (m_sample_size_max == 0 || __byte_count <= m_sample_size_max)) {

        int r = rand();
        if (r > m_sampling * RAND_MAX) {
            release_lock();
            return;
        }

        auto stack_frames = new std::vector<unwindstack::FrameData>;
        unwindstack::do_unwind(*stack_frames);

        if (!stack_frames->empty()) {
            uint64_t stack_hash = hash(*stack_frames);
            ptr_meta.stack_hash = stack_hash;
            stack_meta_t& stack_meta = m_stack_meta[stack_hash];
            stack_meta.size += __byte_count;
            stack_meta.p_stacktraces = stack_frames;
        }
    }

    release_lock();
}

inline void on_release_memory(void *__caller, void *__ptr) {
    acquire_lock();

    if (!m_ptr_meta.count(__ptr)) { // 指针未记录
        release_lock();
        return;
    }

//    LOGD("Yves.debug", "on release ptr = %p", __ptr);

    ptr_meta_t& ptr_meta = m_ptr_meta.at(__ptr);
//    assert(ptr_meta.caller);
//    assert(ptr_meta.size);

    if (!ptr_meta.caller || !m_caller_meta.count(ptr_meta.caller)) {
        LOGE("Yves.debug", " caller null or meta not exist? %zu", m_caller_meta.count(ptr_meta.caller));
        release_lock();
        return;
    }

    caller_meta_t& caller_meta = m_caller_meta.at(ptr_meta.caller);
    if (caller_meta.total_size > ptr_meta.size) { // 减去 caller 的 size
        caller_meta.total_size -= ptr_meta.size;
        caller_meta.pointers.erase(__ptr);
    } else { // 删除 size 为 0 的 caller
        m_caller_meta.erase(ptr_meta.caller);
    }

    if (is_stacktrace_enabled && ptr_meta.stack_hash) {
        stack_meta_t& stack_meta = m_stack_meta.at(ptr_meta.stack_hash);
        if (stack_meta.size > ptr_meta.size) { // 减去同堆栈的 size
            stack_meta.size -= ptr_meta.size;
        } else { // 删除 size 为 0 的堆栈
            m_stack_meta.erase(ptr_meta.stack_hash);
        }
    }

    m_ptr_meta.erase(__ptr);
    release_lock();
}

void *h_malloc(size_t __byte_count) {

    void *ptr = malloc(__byte_count);

    GET_CALLER_ADDR(caller);

    on_acquire_memory(caller, ptr, __byte_count);

    return ptr;
}

void *h_calloc(size_t __item_count, size_t __item_size) {
    void *p = calloc(__item_count, __item_size);

    GET_CALLER_ADDR(caller);

    size_t byte_count = __item_size * __item_count;

    on_acquire_memory(caller, p, byte_count);

    return p;
}

void *h_realloc(void *__ptr, size_t __byte_count) {

    void *p = realloc(__ptr, __byte_count);

    GET_CALLER_ADDR(caller);

    // If ptr is NULL, then the call is equivalent to malloc(size), for all values of size;
    // if size is equal to zero, and ptr is not NULL, then the call is equivalent to free(ptr).
    // Unless ptr is NULL, it must have been returned by an earlier call to malloc(), calloc() or realloc().
    // If the area pointed to was moved, a free(ptr) is done.
    if (!__ptr) { // malloc
        on_acquire_memory(caller, p, __byte_count);
    } else {
        if (!__byte_count) { // free
            on_release_memory(caller, __ptr);
        } else if (__ptr != p) { // moved
            on_release_memory(caller, __ptr);
            on_acquire_memory(caller, p, __byte_count);
        }
    }

    return p;
}

void h_free(void *__ptr) {

    GET_CALLER_ADDR(caller);

    on_release_memory(caller, __ptr);

    free(__ptr);
}

ANDROID_DLOPEN orig_dlopen;

void *h_dlopen(const char *filename,
               int flag,
               const void *extinfo,
               const void *caller_addr) {
    void *ret = (*orig_dlopen)(filename, flag, extinfo, caller_addr);

    if (is_stacktrace_enabled) {
        unwindstack::update_maps();
    }
    xhook_refresh(false);
    srand((unsigned int) time(NULL));
    LOGD("Yves.dlopen", "dlopen %s", filename);
    return ret;
}

static void do_it(std::unordered_map<void *, size_t>::iterator __begin,
                  std::unordered_map<void *, size_t>::iterator __end) {
    for (auto i = __begin; i != __end; i++) {
        Dl_info dl_info;
        dladdr(i->first, &dl_info);
        LOGD("Yves.dump", "key=%p, value=%zu, fname=%s, sname=%s", i->first, i->second,
             dl_info.dli_fname, dl_info.dli_sname);

    }
}

//void dump_old(std::string path) {
//    LOGD("Yves.dump",
//         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> memory dump begin <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
//
//    /******************************* caller so ********************************/
//
//    acquire_lock();
//
//    FILE *log_file = fopen(path.c_str(), "w+");
//    LOGD("Yves.dump", "dump path = %s", path.c_str());
//    if (!log_file) {
//        LOGE("Yves.dump", "open file failed");
//        release_lock();
//        return;
//    }
//
//    if (!m_size_of_caller) {
//        LOGE("Yves.dump", "caller: nothing dump");
//        fflush(log_file);
//        fclose(log_file);
//        release_lock();
//        return;
//    }
//    LOGD("Yves.dump", "caller count = %zu", m_size_of_caller->size());
//    fprintf(log_file, "caller count = %zu\n", m_size_of_caller->size());
//
//    std::unordered_map<std::string, size_t> caller_alloc_size_of_so;
//    std::unordered_map<std::string, std::map<size_t, size_t>> same_size_count_of_so;
//
//    for (auto i = m_size_of_caller->begin(); i != m_size_of_caller->end(); ++i) {
//        auto caller = i->first;
//        auto size = i->second;
//        Dl_info dl_info;
//        dladdr(caller, &dl_info);
//
//        if (dl_info.dli_fname) {
//            // fixme hard coding
////            if (std::string(dl_info.dli_fname).find("com.tencent.mm") != std::string::npos) {
//            caller_alloc_size_of_so[dl_info.dli_fname] += size;
////            } else {
////                LOGD("Yves.dump", "===> so name = %s", dl_info.dli_fname);
////                caller_alloc_size_of_so["other.so"] += size;
////            }
//
//            if (is_group_by_size_enabled) {
//                auto pointers = (*m_pointers_of_caller)[caller];
//                for (auto p = pointers.begin(); p != pointers.end(); ++p) {
//                    size_t ptr_size = (*m_size_of_pointer)[*p];
//                    same_size_count_of_so[dl_info.dli_fname][ptr_size]++;
//                }
//            }
//
//        }
//    }
//
//    std::multimap<size_t, std::string> result_sort_by_size;
//
//    std::transform(caller_alloc_size_of_so.begin(),
//                   caller_alloc_size_of_so.end(),
//                   std::inserter(result_sort_by_size, result_sort_by_size.begin()),
//                   [](std::pair<std::string, size_t> src) {
//                       return std::pair<size_t, std::string>(src.second, src.first);
//                   });
//
//    size_t caller_total_size = 0;
//    for (auto i = result_sort_by_size.rbegin(); i != result_sort_by_size.rend(); ++i) {
//        LOGD("Yves.dump", "so = %s, caller alloc size = %zu", i->second.c_str(), i->first);
//        fprintf(log_file, "caller alloc size = %10zu b, so = %s\n", i->first, i->second.c_str());
//        caller_total_size += i->first;
//
//        if (is_group_by_size_enabled) {
//            auto count_of_size = same_size_count_of_so[i->second];
//            std::multimap<size_t, std::pair<size_t, size_t>> result_sort_by_mul;
//            std::transform(count_of_size.begin(),
//                           count_of_size.end(),
//                           std::inserter(result_sort_by_mul, result_sort_by_mul.begin()),
//                           [](std::pair<size_t, size_t> src) {
//                               return std::pair<size_t, std::pair<size_t, size_t >>(
//                                       src.first * src.second,
//                                       std::pair<size_t, size_t>(src.first, src.second));
//                           });
//            int lines = 20; // fixme hard coding
//            LOGD("Yves.dump", "top %d (size * count):", lines);
//            fprintf(log_file, "top %d (size * count):\n", lines);
//
//            for (auto sc = result_sort_by_mul.rbegin();
//                 sc != result_sort_by_mul.rend() && lines; ++sc, --lines) {
//                auto size = sc->second.first;
//                auto count = sc->second.second;
//                LOGD("Yves.dump", "   size = %10zu b, count = %zu", size, count);
//                fprintf(log_file, "   size = %10zu b, count = %zu\n", size, count);
//            }
//        }
//    }
//
//    LOGD("Yves.dump", "\n---------------------------------------------------");
//    fprintf(log_file, "\n---------------------------------------------------\n");
//    LOGD("Yves.dump", "| caller total size = %zu b", caller_total_size);
//    fprintf(log_file, "| caller total size = %zu b\n", caller_total_size);
//    LOGD("Yves.dump", "---------------------------------------------------\n");
//    fprintf(log_file, "---------------------------------------------------\n\n");
//
//    /******************************* stacktrace ********************************/
//
//    std::unordered_map<std::string, size_t> stack_alloc_size_of_so;
//    std::unordered_map<std::string, std::vector<std::pair<size_t, std::string>>> stacktrace_of_so;
//
//    if (!m_size_of_hash || !m_stacktrace_of_hash) {
//        LOGE("Yves-dump", "stacktrace: nothing dump");
//        fflush(log_file);
//        fclose(log_file);
//        release_lock();
//        return;
//    }
//
//    LOGD("Yves.dump", "hash count = %zu", m_size_of_hash->size());
//    fprintf(log_file, "hash count = %zu\n", m_size_of_hash->size());
//
//    for (auto i = m_size_of_hash->begin(); i != m_size_of_hash->end(); ++i) {
//        auto hash = i->first;
//        auto size = i->second;
////        LOGD("Yves", "hash = %016lx, size = %zu", hash, size);
//        if (m_stacktrace_of_hash->count(hash)) {
//            auto stacktrace = m_stacktrace_of_hash->find(hash)->second;
//
//            std::string caller_so_name;
//            std::stringstream stack_builder;
//            for (auto it = stacktrace->begin(); it != stacktrace->end(); ++it) {
//                Dl_info stack_info;
//                dladdr((void *) it->pc, &stack_info);
//
//                std::string so_name = std::string(stack_info.dli_fname);
//
//                int status = 0;
//                char *demangled_name = abi::__cxa_demangle(stack_info.dli_sname, nullptr, 0,
//                                                           &status);
//
//                stack_builder << "      | "
//                              << "#pc " << std::hex << it->rel_pc << " "
//                              << (demangled_name ? demangled_name : "(null)")
//                              << " (" << stack_info.dli_fname << ")"
//                              << std::endl;
//                // fixme hard coding
//                if (/*so_name.find("com.tencent.mm") == std::string::npos ||*/
//                        so_name.find("libwxperf.so") != std::string::npos ||
//                        !caller_so_name.empty()) {
//                    continue;
//                }
//
//                caller_so_name = so_name;
//                stack_alloc_size_of_so[caller_so_name] += size;
//
//            }
//
//            std::pair<size_t, std::string> stack_size_pair(size, stack_builder.str());
//            stacktrace_of_so[caller_so_name].push_back(stack_size_pair);
//        }
//    }
//
//    for (auto i = stack_alloc_size_of_so.begin(); i != stack_alloc_size_of_so.end(); ++i) {
//        auto so_name = i->first;
//        auto so_alloc_size = i->second;
//
//        LOGD("Yves.dump", "\nmalloc size of so (%s) : remaining size = %zu", so_name.c_str(),
//             so_alloc_size);
//        fprintf(log_file, "\nmalloc size of so (%s) : remaining size = %zu\n", so_name.c_str(),
//                so_alloc_size);
//
//        for (auto it_stack = stacktrace_of_so[so_name].begin();
//             it_stack != stacktrace_of_so[so_name].end(); ++it_stack) {
//
//            LOGD("Yves.dump", "malloc size of the same stack = %zu\n stacktrace : \n%s",
//                 it_stack->first,
//                 it_stack->second.c_str());
//
//            fprintf(log_file, "malloc size of the same stack = %zu\n stacktrace : \n%s\n",
//                    it_stack->first,
//                    it_stack->second.c_str());
//        }
//    }
//
//    fprintf(log_file,
//            "m_size_of_caller[%zu]\nm_size_of_pointer[%zu]\nm_caller_of_pointer[%zu]\nm_pointers_of_caller[%zu]\nm_pc_hash_of_pointer[%zu]\nm_size_of_hash[%zu]\nm_stacktrace_of_hash[%zu]\n",
//            m_size_of_caller->size(), m_size_of_pointer->size(), m_caller_of_pointer->size(),
//            m_pointers_of_caller->size(), m_pc_hash_of_pointer->size(), m_size_of_hash->size(),
//            m_stacktrace_of_hash->size());
//
//    fflush(log_file);
//    fclose(log_file);
//
//    release_lock();
//    LOGD("Yves.dump",
//         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> memory dump end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
//}

void dump(std::string path) {
    LOGD("Yves.dump",
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> memory dump begin <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");

    acquire_lock();

    FILE *log_file = fopen(path.c_str(), "w+");
    LOGD("Yves.dump", "dump path = %s", path.c_str());
    if (!log_file) {
        LOGE("Yves.dump", "open file failed");
        release_lock();
        return;
    }

    if (m_caller_meta.empty()) {
        LOGE("Yves.dump", "caller: nothing dump");
        fflush(log_file);
        fclose(log_file);
        release_lock();
        return;
    }

    LOGD("Yves.dump", "caller count = %zu", m_caller_meta.size());
    fprintf(log_file, "caller count = %zu\n", m_caller_meta.size());

    /******************************* caller so ********************************/

    std::unordered_map<std::string, size_t> caller_alloc_size_of_so;
    std::unordered_map<std::string, std::map<size_t, size_t>> same_size_count_of_so;

    // 按 so 聚类
    for (auto i = m_caller_meta.begin(); i != m_caller_meta.end(); ++i) {
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
            auto ptr_meta = m_ptr_meta.at(*p);
            same_size_count_of_so[dl_info.dli_fname][ptr_meta.size]++;
        }
    }

    // 排序 so
    std::multimap<size_t, std::string> result_sort_by_size;

    std::transform(caller_alloc_size_of_so.begin(),
                   caller_alloc_size_of_so.end(),
                   std::inserter(result_sort_by_size, result_sort_by_size.begin()),
                   [](std::pair<std::string, size_t> src) {
                       return std::pair<size_t, std::string>(src.second, src.first);
                   });

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

    fprintf(log_file, "<void *, ptr_meta_t> m_ptr_meta [%zu * %zu = (%zu)]\n<void *, caller_meta_t> m_caller_meta [%zu * %zu = (%zu)]\n<uint64_t, stack_meta_t> m_stack_meta [%zu * %zu = (%zu)]\n\n",
            sizeof(ptr_meta_t) + sizeof(void *), m_ptr_meta.size(),(sizeof(ptr_meta_t) + sizeof(void *)) * m_ptr_meta.size(),
            sizeof(m_caller_meta) + sizeof(void *), m_caller_meta.size(),(sizeof(m_caller_meta) + sizeof(void *)) * m_caller_meta.size(),
            sizeof(stack_meta_t) + sizeof(uint64_t), m_stack_meta.size(), (sizeof(stack_meta_t) + sizeof(uint64_t)) * m_stack_meta.size());

    /******************************* stacktrace ********************************/
    if (m_stack_meta.empty()) {
        LOGE("Yves-dump", "stacktrace: nothing dump");
        fflush(log_file);
        fclose(log_file);
        release_lock();
        return;
    }

    LOGD("Yves.dump", "hash count = %zu", m_stack_meta.size());
    fprintf(log_file, "hash count = %zu\n", m_stack_meta.size());

    std::unordered_map<std::string, size_t> stack_alloc_size_of_so;
    std::unordered_map<std::string, std::vector<std::pair<size_t, std::string>>> stacktrace_of_so;

    for (auto i = m_stack_meta.begin(); i != m_stack_meta.end(); ++i) {
        auto hash = i->first;
        auto size = i->second.size;
        auto stacktrace = i->second.p_stacktraces;

        std::string caller_so_name;
        std::stringstream stack_builder;
        for (auto it = stacktrace->begin(); it != stacktrace->end(); ++it) {
            Dl_info stack_info;
            dladdr((void *) it->pc, &stack_info);

            std::string so_name = std::string(stack_info.dli_fname);

            int status = 0;
            char *demangled_name = abi::__cxa_demangle(stack_info.dli_sname, nullptr, 0,
                                                       &status);

            stack_builder << "      | "
                          << "#pc " << std::hex << it->rel_pc << " "
                          << (demangled_name ? demangled_name : "(null)")
                          << " (" << stack_info.dli_fname << ")"
                          << std::endl;
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

    fflush(log_file);
    fclose(log_file);

    release_lock();
    LOGD("Yves.dump",
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> memory dump end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
}

#ifndef __LP64__

ORIGINAL_FUNC_PTR(_Znwj);

DEFINE_HOOK_FUN(void*, _Znwj, size_t size) {
//    void * p = ORIGINAL_FUNC_NAME(_Znwj)(size);
    CALL_ORIGIN_FUNC_RET(void*, p, _Znwj, size);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

ORIGINAL_FUNC_PTR(_ZnwjSt11align_val_t);

DEFINE_HOOK_FUN(void*, _ZnwjSt11align_val_t, size_t size, std::align_val_t align_val) {
//    void * p = ORIGINAL_FUNC_NAME(_ZnwjSt11align_val_t)(size, align_val);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnwjSt11align_val_t, size, align_val);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

ORIGINAL_FUNC_PTR(_ZnwjSt11align_val_tRKSt9nothrow_t);

DEFINE_HOOK_FUN(void*, _ZnwjSt11align_val_tRKSt9nothrow_t, size_t size,
                    std::align_val_t align_val, std::nothrow_t const& nothrow) {
//    void * p = ORIGINAL_FUNC_NAME(_ZnwjSt11align_val_tRKSt9nothrow_t)(size, align_val, nothrow);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnwjSt11align_val_tRKSt9nothrow_t, size, align_val, nothrow);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

ORIGINAL_FUNC_PTR(_ZnwjRKSt9nothrow_t);

DEFINE_HOOK_FUN(void*, _ZnwjRKSt9nothrow_t, size_t size, std::nothrow_t const& nothrow) {
//    void * p = ORIGINAL_FUNC_NAME(_ZnwjRKSt9nothrow_t)(size, nothrow);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnwjRKSt9nothrow_t, size, nothrow);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

ORIGINAL_FUNC_PTR(_Znaj);

DEFINE_HOOK_FUN(void*, _Znaj, size_t size) {
//    void * p = ORIGINAL_FUNC_NAME(_Znaj)(size);
    CALL_ORIGIN_FUNC_RET(void*, p, _Znaj, size);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

ORIGINAL_FUNC_PTR(_ZnajSt11align_val_t);

DEFINE_HOOK_FUN(void*, _ZnajSt11align_val_t, size_t size, std::align_val_t align_val) {
//    void * p = ORIGINAL_FUNC_NAME(_ZnajSt11align_val_t)(size, align_val);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnajSt11align_val_t, size, align_val);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

ORIGINAL_FUNC_PTR(_ZnajSt11align_val_tRKSt9nothrow_t);

DEFINE_HOOK_FUN(void*, _ZnajSt11align_val_tRKSt9nothrow_t, size_t size,
                    std::align_val_t align_val, std::nothrow_t const& nothrow) {
//    void * p = ORIGINAL_FUNC_NAME(_ZnajSt11align_val_tRKSt9nothrow_t)(size, align_val, nothrow);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnajSt11align_val_tRKSt9nothrow_t, size, align_val, nothrow);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

ORIGINAL_FUNC_PTR(_ZnajRKSt9nothrow_t);

DEFINE_HOOK_FUN(void*, _ZnajRKSt9nothrow_t, size_t size, std::nothrow_t const& nothrow) {
//    void * p = ORIGINAL_FUNC_NAME(_ZnajRKSt9nothrow_t)(size, nothrow);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnajRKSt9nothrow_t, size, nothrow);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

ORIGINAL_FUNC_PTR(_ZdaPvj);

DEFINE_HOOK_FUN(void, _ZdaPvj, void* ptr, size_t size) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdaPvj)(ptr, size);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvj, ptr, size);
}

ORIGINAL_FUNC_PTR(_ZdaPvjSt11align_val_t);

DEFINE_HOOK_FUN(void, _ZdaPvjSt11align_val_t, void* ptr, size_t size,
                    std::align_val_t align_val) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdaPvjSt11align_val_t)(ptr, size, align_val);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvjSt11align_val_t, ptr, size, align_val);
}

ORIGINAL_FUNC_PTR(_ZdlPvj);

DEFINE_HOOK_FUN(void, _ZdlPvj, void* ptr, size_t size) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdlPvj)(ptr, size);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvj, ptr, size);
}

ORIGINAL_FUNC_PTR(_ZdlPvjSt11align_val_t);

DEFINE_HOOK_FUN(void, _ZdlPvjSt11align_val_t, void* ptr, size_t size,
                    std::align_val_t align_val) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdlPvjSt11align_val_t)(ptr, size, align_val);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvjSt11align_val_t, ptr, size, align_val);
}

#else

ORIGINAL_FUNC_PTR(_Znwm);

DEFINE_HOOK_FUN(void*, _Znwm, size_t size) {
//    void * p = ORIGINAL_FUNC_NAME(_Znwm)(size);
    CALL_ORIGIN_FUNC_RET(void*, p, _Znwm, size);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

ORIGINAL_FUNC_PTR(_ZnwmSt11align_val_t);

DEFINE_HOOK_FUN(void*, _ZnwmSt11align_val_t, size_t size, std::align_val_t align_val) {
//    void * p = ORIGINAL_FUNC_NAME(_ZnwmSt11align_val_t)(size, align_val);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnwmSt11align_val_t, size, align_val);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

ORIGINAL_FUNC_PTR(_ZnwmSt11align_val_tRKSt9nothrow_t);

DEFINE_HOOK_FUN(void*, _ZnwmSt11align_val_tRKSt9nothrow_t, size_t size,
                std::align_val_t align_val, std::nothrow_t const &nothrow) {
//    DO_HOOK_ACQUIRE(_ZnwmSt11align_val_tRKSt9nothrow_t, size,align_val,nothrow)
//    void * p = ORIGINAL_FUNC_NAME(_ZnwmSt11align_val_tRKSt9nothrow_t)(size,align_val,nothrow);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnwmSt11align_val_tRKSt9nothrow_t, size, align_val, nothrow);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

ORIGINAL_FUNC_PTR(_ZnwmRKSt9nothrow_t);

DEFINE_HOOK_FUN(void*, _ZnwmRKSt9nothrow_t, size_t size, std::nothrow_t const &nothrow) {
//    DO_HOOK_ACQUIRE(_ZnwmRKSt9nothrow_t, size, nothrow)
//    void * p = ORIGINAL_FUNC_NAME(_Znwm)(size);
    CALL_ORIGIN_FUNC_RET(void*, p, _Znwm, size);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

ORIGINAL_FUNC_PTR(_Znam);

DEFINE_HOOK_FUN(void*, _Znam, size_t size) {
//    void * p = ORIGINAL_FUNC_NAME(_Znam)(size);
    CALL_ORIGIN_FUNC_RET(void*, p, _Znam, size);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

ORIGINAL_FUNC_PTR(_ZnamSt11align_val_t);

DEFINE_HOOK_FUN(void*, _ZnamSt11align_val_t, size_t size, std::align_val_t align_val) {
//    void * p = ORIGINAL_FUNC_NAME(_ZnamSt11align_val_t)(size,align_val);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnamSt11align_val_t, size, align_val);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

ORIGINAL_FUNC_PTR(_ZnamSt11align_val_tRKSt9nothrow_t);

DEFINE_HOOK_FUN(void*, _ZnamSt11align_val_tRKSt9nothrow_t, size_t size,
                std::align_val_t align_val, std::nothrow_t const &nothrow) {
//    void * p = ORIGINAL_FUNC_NAME(_ZnamSt11align_val_tRKSt9nothrow_t)(size, align_val, nothrow);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnamSt11align_val_tRKSt9nothrow_t, size, align_val, nothrow);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

ORIGINAL_FUNC_PTR(_ZnamRKSt9nothrow_t);

DEFINE_HOOK_FUN(void*, _ZnamRKSt9nothrow_t, size_t size, std::nothrow_t const &nothrow) {
//    void * p = ORIGINAL_FUNC_NAME(_ZnamRKSt9nothrow_t)(size, nothrow);
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnamRKSt9nothrow_t, size, nothrow);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

ORIGINAL_FUNC_PTR(_ZdlPvm);

DEFINE_HOOK_FUN(void, _ZdlPvm, void *ptr, size_t size) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdlPvm)(ptr, size);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvm, ptr, size);
}

ORIGINAL_FUNC_PTR(_ZdlPvmSt11align_val_t);

DEFINE_HOOK_FUN(void, _ZdlPvmSt11align_val_t, void *ptr, size_t size,
                std::align_val_t align_val) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdlPvmSt11align_val_t)(ptr, size, align_val);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvmSt11align_val_t, ptr, size, align_val);
}


ORIGINAL_FUNC_PTR(_ZdaPvm);

DEFINE_HOOK_FUN(void, _ZdaPvm, void *ptr, size_t size) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdaPvm)(ptr, size);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvm, ptr, size);
}

ORIGINAL_FUNC_PTR(_ZdaPvmSt11align_val_t);

DEFINE_HOOK_FUN(void, _ZdaPvmSt11align_val_t, void *ptr, size_t size,
                std::align_val_t align_val) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdaPvmSt11align_val_t)(ptr, size, align_val);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvmSt11align_val_t, ptr, size, align_val);
}

#endif

ORIGINAL_FUNC_PTR(strdup);

DEFINE_HOOK_FUN(char*, strdup, const char *str) {
//    char * p = ORIGINAL_FUNC_NAME(strdup)(str);
    CALL_ORIGIN_FUNC_RET(char *, p, strdup, str);
    return p;
}

ORIGINAL_FUNC_PTR(strndup);

DEFINE_HOOK_FUN(char*, strndup, const char *str, size_t n) {
//    char * p = ORIGINAL_FUNC_NAME(strndup)(str, n);
    CALL_ORIGIN_FUNC_RET(char *, p, strndup, str, n);
    return p;
}

ORIGINAL_FUNC_PTR(_ZdlPv);

DEFINE_HOOK_FUN(void, _ZdlPv, void *p) {
    DO_HOOK_RELEASE(p);
//    ORIGINAL_FUNC_NAME(_ZdlPv)(p);
    CALL_ORIGIN_FUNC_VOID(_ZdlPv, p);
}

ORIGINAL_FUNC_PTR(_ZdlPvSt11align_val_t);

DEFINE_HOOK_FUN(void, _ZdlPvSt11align_val_t, void *ptr, std::align_val_t align_val) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdlPvSt11align_val_t)(ptr, align_val);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvSt11align_val_t, ptr, align_val);
}

ORIGINAL_FUNC_PTR(_ZdlPvSt11align_val_tRKSt9nothrow_t);

DEFINE_HOOK_FUN(void, _ZdlPvSt11align_val_tRKSt9nothrow_t, void *ptr,
                std::align_val_t align_val, std::nothrow_t const &nothrow) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdlPvSt11align_val_tRKSt9nothrow_t)(ptr, align_val, nothrow);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvSt11align_val_tRKSt9nothrow_t, ptr, align_val, nothrow);
}

ORIGINAL_FUNC_PTR(_ZdlPvRKSt9nothrow_t);

DEFINE_HOOK_FUN(void, _ZdlPvRKSt9nothrow_t, void *ptr, std::nothrow_t const &nothrow) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdlPvRKSt9nothrow_t)(ptr, nothrow);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvRKSt9nothrow_t, ptr, nothrow);
}


ORIGINAL_FUNC_PTR(_ZdaPv);

DEFINE_HOOK_FUN(void, _ZdaPv, void *ptr) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdaPv)(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdaPv, ptr);
}

ORIGINAL_FUNC_PTR(_ZdaPvSt11align_val_t);

DEFINE_HOOK_FUN(void, _ZdaPvSt11align_val_t, void *ptr, std::align_val_t align_val) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdaPvSt11align_val_t)(ptr, align_val);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvSt11align_val_t, ptr, align_val);
}

ORIGINAL_FUNC_PTR(_ZdaPvSt11align_val_tRKSt9nothrow_t);

DEFINE_HOOK_FUN(void, _ZdaPvSt11align_val_tRKSt9nothrow_t, void *ptr,
                std::align_val_t align_val, std::nothrow_t const &nothrow) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdaPvSt11align_val_tRKSt9nothrow_t)(ptr, align_val, nothrow);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvSt11align_val_tRKSt9nothrow_t, ptr, align_val, nothrow);
}

ORIGINAL_FUNC_PTR(_ZdaPvRKSt9nothrow_t);

DEFINE_HOOK_FUN(void, _ZdaPvRKSt9nothrow_t, void *ptr, std::nothrow_t const &nothrow) {
    DO_HOOK_RELEASE(ptr);
//    ORIGINAL_FUNC_NAME(_ZdaPvRKSt9nothrow_t)(ptr, nothrow);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvRKSt9nothrow_t, ptr, nothrow);
}