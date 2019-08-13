//
// Created by Yves on 2019-08-08.
//

#include <malloc.h>
#include <dlfcn.h>
#include <unistd.h>
#include <unordered_map>
#include <random>
#include <xhook.h>
#include <sstream>
#include "MemoryHook.h"
#include "lock.h"
#include "StackTrace.h"
#include "utils.h"
#include "unwindstack/Unwinder.h"

std::unordered_map<void *, size_t> *m_size_of_caller;
std::unordered_map<void *, size_t> *m_size_of_pointer;
std::unordered_map<void *, void *> *m_caller_of_pointer;

std::unordered_map<void *, uint64_t> *m_pc_hash_of_pointer;
std::unordered_map<uint64_t, size_t> *m_size_of_hash;
std::unordered_map<uint64_t, std::vector<unwindstack::FrameData> *> *m_stacktrace_of_hash;

bool is_stacktrace_enabled = false;

void enableStacktrace(bool enable) {
    is_stacktrace_enabled = enable;
}

static inline void on_acquire_memory(void *__caller, void *__ptr, size_t __byte_count) {
//    LOGD("Yves", "on acquire memory, ptr = (%p)", __ptr);
    init_if_necessary();

    (*m_size_of_caller)[__caller] += __byte_count;
    (*m_size_of_pointer)[__ptr] = __byte_count;
    (*m_caller_of_pointer)[__ptr] = __caller;

    if (is_stacktrace_enabled) {
        auto stack_frames = new std::vector<unwindstack::FrameData>;
        unwindstack::do_unwind((*stack_frames));

        if (!stack_frames->empty()) {
            uint64_t stack_hash = hash((*stack_frames));

            (*m_pc_hash_of_pointer)[__ptr] = stack_hash;
            (*m_stacktrace_of_hash)[stack_hash] = stack_frames;
            (*m_size_of_hash)[stack_hash] += __byte_count;
        }
    }
}

inline void on_release_memory(void *caller, void *__ptr) {
    init_if_necessary();
    // with caller

    if (!m_size_of_pointer->count(__ptr)) {
//        LOGD("Yves.ERROR", "can NOT find size of given pointer(%p), caller(%p)", __ptr, caller);
        return;
    }

    auto ptr_size = m_size_of_pointer->at(__ptr);
    if (m_caller_of_pointer->count(__ptr)) {
        auto caller = m_caller_of_pointer->at(__ptr);
        if (m_size_of_caller->count(caller)) {
            auto caller_size = m_size_of_caller->at(caller);
            if (caller_size > ptr_size) {
                (*m_size_of_caller)[caller] = caller_size - ptr_size;
            } else {
                m_size_of_caller->erase(caller);
            }
        }
    }

    // with stacktrace
    if (is_stacktrace_enabled && m_pc_hash_of_pointer->count(__ptr)) {
        auto pc_hash = m_pc_hash_of_pointer->at(__ptr);
        if (m_size_of_hash->count(pc_hash)) {
            auto hash_size = m_size_of_hash->at(pc_hash);
            if (hash_size > ptr_size) {
                (*m_size_of_hash)[pc_hash] = hash_size - ptr_size;
            } else {
                m_size_of_hash->erase(pc_hash);
            }
        }
    }
}

void *h_malloc(size_t __byte_count) {
    acquire_lock();

    void *ptr = malloc(__byte_count);

    GET_CALLER_ADDR(caller)

    on_acquire_memory(caller, ptr, __byte_count);

    release_lock();
    return ptr;
}

void *h_calloc(size_t __item_count, size_t __item_size) {
    acquire_lock();
    void *p = calloc(__item_count, __item_size);

    GET_CALLER_ADDR(caller)

    size_t byte_count = __item_size * __item_count;

    on_acquire_memory(caller, p, byte_count);

    release_lock();
    return p;
}

void *h_realloc(void *__ptr, size_t __byte_count) {
    acquire_lock();

    void *p = realloc(__ptr, __byte_count);

    GET_CALLER_ADDR(caller)

    on_acquire_memory(caller, p, __byte_count);

    release_lock();
    return p;
}

void h_free(void *__ptr) {
    acquire_lock();

    GET_CALLER_ADDR(caller);

    on_release_memory(caller, __ptr);

    free(__ptr);
    release_lock();
}

ANDROID_DLOPEN p_oldfun;

void *h_dlopen(const char *filename,
               int flag,
               const void *extinfo,
               const void *caller_addr) {
    void *ret = (*p_oldfun)(filename, flag, extinfo, caller_addr);

    unwindstack::update_maps();
    xhook_refresh(false);
//    LOGD("Yves.dlopen", "dlopen %s", filename);
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

void dump() {
    LOGD("Yves.dump", "~~~~~~ dump begin ~~~~~~");

    FILE *log_file = fopen("/sdcard/memory_hook.log", "w+");
    if (!log_file) {
        LOGE("Yves.dump", "open file failed");
        return;
    }

    LOGD("Yves.dump", "caller count = %zu", m_size_of_caller->size());
    fprintf(log_file, "caller count = %zu\n", m_size_of_caller->size());

    std::unordered_map<std::string, size_t> caller_alloc_size_of_so;

    for (auto i = m_size_of_caller->begin(); i != m_size_of_caller->end(); ++i) {
        auto caller = i->first;
        auto size = i->second;
        Dl_info dl_info;
        dladdr(caller, &dl_info);

//        LOGD("Yves.dump", "key = %p, value = %zu, caller so = %s, caller function = %s",
//             caller, size, dl_info.dli_fname, dl_info.dli_sname);
//        fprintf(log_file, "key = %p, value = %zu, caller so = %s, caller function = %s\n",
//                caller, size, dl_info.dli_fname, dl_info.dli_sname);

        if (dl_info.dli_fname) {
            if (std::string(dl_info.dli_fname).find("com.tencent.mm") != std::string::npos) {
                caller_alloc_size_of_so[dl_info.dli_fname] += size;
            } else {
                caller_alloc_size_of_so["other.so"] += size;
            }
        }
    }

    for (auto i = caller_alloc_size_of_so.begin(); i != caller_alloc_size_of_so.end(); ++i) {
        LOGD("Yves.dump", "so = %s, caller alloc size = %zu", i->first.c_str(), i->second);
        fprintf(log_file, "caller alloc size = %10zu, so = %s\n", i->second, i->first.c_str());
    }

    std::unordered_map<std::string, size_t> stack_alloc_size_of_so;
    std::unordered_map<std::string, std::vector<std::pair<size_t, std::string>>> stacktrace_of_so;

    LOGD("Yves.dump", "hash count = %zu", m_size_of_hash->size());
    fprintf(log_file, "hash count = %zu\n", m_size_of_hash->size());

    for (auto i = m_size_of_hash->begin(); i != m_size_of_hash->end(); ++i) {
        auto hash = i->first;
        auto size = i->second;
//        LOGD("Yves", "hash = %016lx, size = %zu", hash, size);
        if (m_stacktrace_of_hash->count(hash)) {
            auto stacktrace = m_stacktrace_of_hash->find(hash)->second;

            std::string custom_so_name;
            std::stringstream stack_builder;
            for (auto it = stacktrace->begin(); it != stacktrace->end(); ++it) {
                Dl_info stack_info;
                dladdr((void *) it->pc, &stack_info);

                std::string so_name = std::string(stack_info.dli_fname);

                stack_builder << "      | "
                              << (stack_info.dli_sname ? stack_info.dli_sname : "null")
                              << " (" << stack_info.dli_fname << ")"
                              << std::endl;
                if (so_name.find("com.tencent.mm") == std::string::npos ||
                    so_name.find("libwxperf.so") != std::string::npos ||
                    !custom_so_name.empty()) {
                    continue;
                }

                custom_so_name = so_name;
                stack_alloc_size_of_so[custom_so_name] += size;

                LOGD("Yves.dump", "so = %s, remaining size = %zu", so_name.c_str(), size);
                fprintf(log_file, "so = %s, remaining size = %zu\n", so_name.c_str(), size);
            }

            std::pair<size_t, std::string> stack_size_pair(size, stack_builder.str());
            stacktrace_of_so[custom_so_name].push_back(stack_size_pair);
        }
    }

    for (auto i = stack_alloc_size_of_so.begin(); i != stack_alloc_size_of_so.end(); ++i) {
        auto so_name = i->first;
        auto so_alloc_size = i->second;

        LOGD("Yves.dump", "\nmalloc size of so (%s) : size = %zu", so_name.c_str(), so_alloc_size);
        fprintf(log_file, "\nmalloc size of so (%s) : size = %zu\n", so_name.c_str(),
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

    LOGD("Yves.dump", "~~~~~~ dump end ~~~~~~");
}

void init_if_necessary() {
    if (!m_size_of_caller) {
        LOGD("Yves", "init size of caller");
        m_size_of_caller = new std::unordered_map<void *, size_t>;
    }

    if (!m_size_of_pointer) {
        m_size_of_pointer = new std::unordered_map<void *, size_t>;
    }

    if (!m_caller_of_pointer) {
        m_caller_of_pointer = new std::unordered_map<void *, void *>;
    }

    if (!m_pc_hash_of_pointer) {
        m_pc_hash_of_pointer = new std::unordered_map<void *, uint64_t>;
    }
//
//    if (!m_size_of_so) {
//        m_size_of_so = new std::unordered_map<std::string, size_t>;
//    }

    if (!m_size_of_hash) {
        m_size_of_hash = new std::unordered_map<uint64_t, size_t>;
    }

    if (!m_stacktrace_of_hash) {
        m_stacktrace_of_hash = new std::unordered_map<uint64_t, std::vector<unwindstack::FrameData> *>;
    }
}


//uint64_t stacktrace_hash(uint64_t *stacktrace, size_t size) {
//    uint64_t sum = 0;
//
//    for (int i = 0; i < size; ++i) {
//        sum += stacktrace[i];
//    }
//
//    return sum;
//}

