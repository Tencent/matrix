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

std::unordered_map<void *, size_t> *m_size_of_caller;
std::unordered_map<void *, size_t> *m_size_of_pointer;
std::unordered_map<void *, void *> *m_caller_of_pointer;
std::unordered_map<void *, std::unordered_set<void *>> *m_pointers_of_caller;

std::unordered_map<void *, uint64_t> *m_pc_hash_of_pointer;
std::unordered_map<uint64_t, size_t> *m_size_of_hash;
std::unordered_map<uint64_t, std::vector<unwindstack::FrameData> *> *m_stacktrace_of_hash;

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

static inline void init_if_necessary() {
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

    if (!m_pointers_of_caller) {
        m_pointers_of_caller = new std::unordered_map<void *, std::unordered_set<void *>>;
    }
}

static inline void on_acquire_memory(void *__caller, void *__ptr, size_t __byte_count) {
//    LOGD("Yves", "on acquire memory, ptr = (%p)", __ptr);
    init_if_necessary();

    (*m_size_of_caller)[__caller] += __byte_count;
    (*m_size_of_pointer)[__ptr] = __byte_count;
    (*m_caller_of_pointer)[__ptr] = __caller;

    if (is_group_by_size_enabled) {
        (*m_pointers_of_caller)[__caller].insert(__ptr);
    }


    if (is_stacktrace_enabled &&
        (m_sample_size_min == 0 || __byte_count >= m_sample_size_min) &&
        (m_sample_size_max == 0 || __byte_count <= m_sample_size_max)) {

        int r = rand();

        if (r > m_sampling * RAND_MAX) {
            return;
        }

        LOGD("Yves", "do unwind");
        auto stack_frames = new std::vector<unwindstack::FrameData>;
        unwindstack::do_unwind(*stack_frames);

        if (!stack_frames->empty()) {
            uint64_t stack_hash = hash((*stack_frames));

            (*m_pc_hash_of_pointer)[__ptr] = stack_hash;
            (*m_stacktrace_of_hash)[stack_hash] = stack_frames;
            (*m_size_of_hash)[stack_hash] += __byte_count;
        }
    }
}

inline void on_release_memory(void *__caller, void *__ptr) {
    init_if_necessary();
    // with caller

    if (!m_size_of_pointer->count(__ptr)) {
//        LOGD("Yves.ERROR", "can NOT find size of given pointer(%p), caller(%p)",
//                __ptr, m_caller_of_pointer->count(__ptr) ? (void *)m_size_of_caller->at(__ptr) : 0);
        return;
    }

    auto ptr_size = m_size_of_pointer->at(__ptr);
    if (m_caller_of_pointer->count(__ptr)) {
        auto alloc_caller = m_caller_of_pointer->at(__ptr);

        if (m_size_of_caller->count(alloc_caller)) {
            auto caller_size = m_size_of_caller->at(alloc_caller);

            if (caller_size > ptr_size) {
                (*m_size_of_caller)[alloc_caller] = caller_size - ptr_size;
            } else {
                m_size_of_caller->erase(alloc_caller);
                if (is_group_by_size_enabled) {
                    m_pointers_of_caller->erase(alloc_caller);
                }
            }
        }

        if (is_group_by_size_enabled) {
            (*m_pointers_of_caller)[alloc_caller].erase(__ptr);
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

void dump(std::string path) {
    LOGD("Yves.dump",
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> memory dump begin <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");

    /******************************* caller so ********************************/

    acquire_lock();

    FILE *log_file = fopen(path.c_str(), "w+");
    LOGD("Yves.dump", "dump path = %s", path.c_str());
    if (!log_file) {
        LOGE("Yves.dump", "open file failed");
        release_lock();
        return;
    }

    if (!m_size_of_caller) {
        LOGE("Yves.dump", "caller: nothing dump");
        fflush(log_file);
        fclose(log_file);
        release_lock();
        return;
    }
    LOGD("Yves.dump", "caller count = %zu", m_size_of_caller->size());
    fprintf(log_file, "caller count = %zu\n", m_size_of_caller->size());

    std::unordered_map<std::string, size_t> caller_alloc_size_of_so;
    std::unordered_map<std::string, std::map<size_t, size_t>> same_size_count_of_so;

    for (auto i = m_size_of_caller->begin(); i != m_size_of_caller->end(); ++i) {
        auto caller = i->first;
        auto size = i->second;
        Dl_info dl_info;
        dladdr(caller, &dl_info);

        if (dl_info.dli_fname) {
            // fixme hard coding
//            if (std::string(dl_info.dli_fname).find("com.tencent.mm") != std::string::npos) {
            caller_alloc_size_of_so[dl_info.dli_fname] += size;
//            } else {
//                LOGD("Yves.dump", "===> so name = %s", dl_info.dli_fname);
//                caller_alloc_size_of_so["other.so"] += size;
//            }

            if (is_group_by_size_enabled) {
                auto pointers = (*m_pointers_of_caller)[caller];
                for (auto p = pointers.begin(); p != pointers.end(); ++p) {
                    size_t ptr_size = (*m_size_of_pointer)[*p];
                    same_size_count_of_so[dl_info.dli_fname][ptr_size]++;
                }
            }

        }
    }

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

        if (is_group_by_size_enabled) {
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
    }

    LOGD("Yves.dump", "\n---------------------------------------------------");
    fprintf(log_file, "\n---------------------------------------------------\n");
    LOGD("Yves.dump", "| caller total size = %zu b", caller_total_size);
    fprintf(log_file, "| caller total size = %zu b\n", caller_total_size);
    LOGD("Yves.dump", "---------------------------------------------------\n");
    fprintf(log_file, "---------------------------------------------------\n\n");

    /******************************* stacktrace ********************************/

    std::unordered_map<std::string, size_t> stack_alloc_size_of_so;
    std::unordered_map<std::string, std::vector<std::pair<size_t, std::string>>> stacktrace_of_so;

    if (!m_size_of_hash || !m_stacktrace_of_hash) {
        LOGE("Yves-dump", "stacktrace: nothing dump");
        fflush(log_file);
        fclose(log_file);
        release_lock();
        return;
    }

    LOGD("Yves.dump", "hash count = %zu", m_size_of_hash->size());
    fprintf(log_file, "hash count = %zu\n", m_size_of_hash->size());

    for (auto i = m_size_of_hash->begin(); i != m_size_of_hash->end(); ++i) {
        auto hash = i->first;
        auto size = i->second;
//        LOGD("Yves", "hash = %016lx, size = %zu", hash, size);
        if (m_stacktrace_of_hash->count(hash)) {
            auto stacktrace = m_stacktrace_of_hash->find(hash)->second;

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
    }

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


//uint64_t stacktrace_hash(uint64_t *stacktrace, size_t size) {
//    uint64_t sum = 0;
//
//    for (int i = 0; i < size; ++i) {
//        sum += stacktrace[i];
//    }
//
//    return sum;
//}

