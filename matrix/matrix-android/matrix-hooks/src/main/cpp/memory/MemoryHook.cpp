/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Created by Yves on 2019-08-08.
//

#include <malloc.h>
#include <unistd.h>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <list>
#include <map>
#include <memory>
#include <thread>
#include <random>
#include <xhook.h>
#include <sstream>
#include <cxxabi.h>
#include <sys/mman.h>
#include <mutex>
#include <condition_variable>
#include <shared_mutex>
#include <cJSON.h>
#include <Log.h>
#include <unwindstack/Unwinder.h>
#include <common/ThreadPool.h>
#include <backtrace/BacktraceDefine.h>
#include <common/ProfileRecord.h>
#include "MemoryHookFunctions.h"
#include "Utils.h"
#include "MemoryHookMetas.h"
#include "MemoryHook.h"

#if USE_MEMORY_MESSAGE_QUEUE == false
static memory_meta_container m_memory_meta_container;
#else
static memory_meta_container_2 m_memory_meta_container;
#endif

static bool is_stacktrace_enabled = false;

static size_t m_tracing_alloc_size_min = 0;
static size_t m_tracing_alloc_size_max = 0;

static size_t m_stacktrace_log_threshold;

typedef enum : uint8_t {
    message_type_nil = 0,
    message_type_allocation = 1,
    message_type_deletion = 2,
    message_type_mmap = 3,
    message_type_munmap = 4
} message_type;

typedef wechat_backtrace::BacktraceFixed<MEMHOOK_BACKTRACE_MAX_FRAMES> memory_backtrace_t;

typedef struct {
    uintptr_t ptr = 0;
    uintptr_t caller = 0;
    size_t size = 0;
    memory_backtrace_t backtrace {};
} allocation_message_t;

typedef struct {
    uintptr_t ptr = 0;
} deletion_message_t;

typedef struct {
    union {
        uintptr_t index;
        uintptr_t ptr;
    };
    message_type type = message_type_nil;
} message_t;

class BufferQueue {

public:
    BufferQueue(size_t size_augment) {
        size_augment_ = size_augment;
        buffer_realloc();
    };

    ~BufferQueue() {
        if (message_queue_) {
            free(message_queue_);
            message_queue_ = nullptr;
        }
        if (alloc_message_queue_) {
            free(alloc_message_queue_);
            alloc_message_queue_ = nullptr;
        }
    };

    allocation_message_t * enqueue_allocation_message(bool is_mmap) {
        if (msg_queue_idx >= msg_queue_size_ || alloc_msg_queue_idx >= alloc_queue_size_) {
            buffer_realloc();
        }

        message_t *msg_idx = &message_queue_[msg_queue_idx++];
        msg_idx->type = is_mmap ? message_type_mmap : message_type_allocation;
        msg_idx->index = alloc_msg_queue_idx;

        allocation_message_t * buffer = &alloc_message_queue_[alloc_msg_queue_idx++];
        *buffer = {};
        return buffer;
    }

    void enqueue_deletion_message(uintptr_t ptr, bool is_munmap) {

        // Fast path.
        if (msg_queue_idx > 0) {
            message_t *msg_idx = &message_queue_[msg_queue_idx - 1];
            if ((msg_idx->type == message_type_allocation || msg_idx->type == message_type_mmap)
                    && alloc_message_queue_[msg_idx->index].ptr == ptr) {
                msg_idx->type = message_type_nil;
                msg_queue_idx--;
                alloc_msg_queue_idx--;
                return;
            }
        }

        if (msg_queue_idx >= msg_queue_size_) {
            buffer_realloc();
        }

        message_t *msg_idx = &message_queue_[msg_queue_idx++];
        msg_idx->type = is_munmap ? message_type_munmap : message_type_deletion;
        msg_idx->ptr = ptr;
    }

    void reset() {
        msg_queue_idx = 0;
        alloc_msg_queue_idx = 0;
    }

    void process(std::function<void(message_t*, allocation_message_t*)> callback) {
        for (size_t i = 0; i < msg_queue_idx; i++) {
            message_t * message = &message_queue_[i];
            allocation_message_t * allocation_message = nullptr;
            if (message->type == message_type_allocation || message->type == message_type_mmap) {
                allocation_message = &alloc_message_queue_[message->index];
            }
            callback(message, allocation_message);
        }
    }

private:

    void buffer_realloc() {
        alloc_queue_size_ += size_augment_;

        void * buffer = realloc(alloc_message_queue_, sizeof(allocation_message_t) * alloc_queue_size_);
        alloc_message_queue_ = static_cast<allocation_message_t *>(buffer);

        msg_queue_size_ = alloc_queue_size_ * 1.5f;
        buffer = realloc(message_queue_, sizeof(allocation_message_t) * msg_queue_size_);
        message_queue_ = static_cast<message_t *>(buffer);
    }

    size_t size_augment_ = 0;

    size_t msg_queue_size_ = 0;
    size_t msg_queue_idx = 0;
    message_t * message_queue_ = nullptr;

    size_t alloc_queue_size_ = 0;
    size_t alloc_msg_queue_idx = 0;
    allocation_message_t * alloc_message_queue_ = nullptr;

};

typedef struct {
    BufferQueue * queue = nullptr;
    std::mutex mutex;
} buffer_queue_container_t;

static const unsigned int MAX_PTR_SLOT   = 1 << 10;
static const unsigned int PTR_MASK       = MAX_PTR_SLOT - 1;

static inline size_t memory_ptr_hash(uintptr_t _key) {
    return static_cast<size_t>((_key ^ (_key >> 16)) & PTR_MASK);
}

class BufferContainer {
public:
    BufferContainer() {
        containers_.reserve(MAX_PTR_SLOT);
        for (int i = 0; i < MAX_PTR_SLOT; ++i) {
            containers_[i] = new buffer_queue_container_t();
        }
    };

    ~BufferContainer() {
        for (auto container : containers_) {
            if (container->queue) {
                delete container->queue;
            }
            delete container;
        }

        if (queue_swapped_) {
            delete queue_swapped_;
        }
    };

    std::vector<buffer_queue_container_t *> containers_;

    BufferQueue * queue_swapped_ = nullptr;

    bool processing = false;

};

#define SIZE_AUGMENT 1024
#define TEST_LOG_WARN(fmt, ...) __android_log_print(ANDROID_LOG_WARN,  "TestHook", fmt, ##__VA_ARGS__)

static void process_routine(BufferContainer *this_) {
    while(true) {
        if (!this_->queue_swapped_) this_->queue_swapped_ = new BufferQueue(SIZE_AUGMENT);

        for (auto container : this_->containers_) {
            TEST_LOG_WARN("process_routine ... ");
            BufferQueue * swapped = nullptr;
            {
                std::lock_guard<std::mutex> lock(container->mutex);
                if (container->queue) {
                    swapped = container->queue;
                    container->queue = this_->queue_swapped_;
                }
            }
            if (swapped) {
                TEST_LOG_WARN("Swapped ... ");
                swapped->process([](message_t* message, allocation_message_t* allocation_message) {
                    if (message->type == message_type_allocation || message->type == message_type_mmap) {
                        if (UNLIKELY(allocation_message == nullptr)) return;
                        uint64_t stack_hash = 0;
                        if (allocation_message->backtrace.frame_size != 0) {
                            stack_hash = hash_frames(allocation_message->backtrace.frames, allocation_message->backtrace.frame_size);
                        }
                        m_memory_meta_container.insert(
                                reinterpret_cast<const void *>(allocation_message->ptr),
                                stack_hash,
                                [&](ptr_meta_t *ptr_meta, stack_meta_t *stack_meta) {
                                    ptr_meta->ptr     = reinterpret_cast<void *>(allocation_message->ptr);
                                    ptr_meta->size    = allocation_message->size;
                                    ptr_meta->caller  = reinterpret_cast<void *>(allocation_message->caller);
                                    ptr_meta->is_mmap = message->type == message_type_mmap;

                                    if (UNLIKELY(!stack_meta)) {
                                        return;
                                    }

                                    stack_meta->size += allocation_message->size;
                                    if (stack_meta->backtrace.frame_size == 0 && allocation_message->backtrace.frame_size != 0) {
                                        stack_meta->backtrace = allocation_message->backtrace;
                                        stack_meta->caller    = reinterpret_cast<void *>(allocation_message->caller);
                                    }
                                });
                    } else if (message->type == message_type_deletion || message->type == message_type_munmap) {
                        m_memory_meta_container.erase(
                                reinterpret_cast<const void *>(message->ptr));
                    }
                });

                swapped->reset();
                this_->queue_swapped_ = swapped;
            }
        }

        usleep(100000);
    }
}

static BufferContainer m_memory_messages_containers_;
void start_process() {
    if (m_memory_messages_containers_.processing) return;
    m_memory_messages_containers_.processing = true;
//        auto th = new std::thread(&BufferContainer::process_routine);
    pthread_t thread;
    pthread_create(&thread, nullptr, reinterpret_cast<void *(*)(void *)>(process_routine), &m_memory_messages_containers_);
//    pthread_detach(thread);   // TODO
}

void memory_hook_init() {
    start_process();
}

void enable_stacktrace(bool enable) {
    is_stacktrace_enabled = enable;
}

void set_stacktrace_log_threshold(size_t threshold) {
    m_stacktrace_log_threshold = threshold;
}

void set_tracing_alloc_size_range(size_t min, size_t max) {
    m_tracing_alloc_size_min = min;
    m_tracing_alloc_size_max = max;
}

static inline bool should_do_unwind(size_t byte_count) {
    return ((m_tracing_alloc_size_min == 0 || byte_count >= m_tracing_alloc_size_min)
            && (m_tracing_alloc_size_max == 0 || byte_count <= m_tracing_alloc_size_max));
}
static inline void do_unwind(wechat_backtrace::Frame *frames, const size_t max_frames,
                                  size_t &frame_size) {
#ifndef FAKE_BACKTRACE_DATA
    fake_unwind(frames, max_frames, frame_size);
#else
    ON_RECORD_START(durations);
    wechat_backtrace::unwind_adapter(frames, max_frames, frame_size);
    ON_RECORD_END(durations);
    ON_MEMORY_BACKTRACE((uint64_t) ptr, &backtrace, (uint64_t) durations);
#endif
}


static inline void on_acquire_memory(
        void *caller,
        void *ptr,
        size_t byte_count,
        bool is_mmap) {

    if (UNLIKELY(!ptr)) {
        LOGE(TAG, "on_acquire_memory: invalid pointer");
        return;
    }

    ON_MEMORY_ALLOCATED((uint64_t) ptr, byte_count);

#if USE_MEMORY_MESSAGE_QUEUE == true
    buffer_queue_container_t * container = m_memory_messages_containers_.containers_[memory_ptr_hash((uintptr_t) ptr)];

    std::lock_guard<std::mutex> lock(container->mutex);
    if (!container->queue) {
        container->queue = new BufferQueue(SIZE_AUGMENT);
    }
    auto message = container->queue->enqueue_allocation_message(is_mmap);
    message->ptr = reinterpret_cast<uintptr_t>(ptr);
    message->size = byte_count;
    message->caller = reinterpret_cast<uintptr_t>(caller);
    if (LIKELY(is_stacktrace_enabled && should_do_unwind(byte_count))) {
        do_unwind(message->backtrace.frames, message->backtrace.max_frames, message->backtrace.frame_size);
    }
#else
    MemoryHookBacktrace backtrace;
    uint64_t            stack_hash = 0;
    if (LIKELY(is_stacktrace_enabled && should_do_unwind(byte_count))) {
        do_unwind(backtrace.frames, backtrace.max_frames, backtrace.frame_size);
        stack_hash = hash_frames(backtrace.frames, backtrace.frame_size);
    }

    m_memory_meta_container.insert(
            ptr,
            stack_hash,
            [&](ptr_meta_t *ptr_meta, stack_meta_t *stack_meta) {
               ptr_meta->ptr     = ptr;
               ptr_meta->size    = byte_count;
               ptr_meta->caller  = caller;
               ptr_meta->is_mmap = is_mmap;

               if (UNLIKELY(!stack_meta)) {
                   return;
               }

               stack_meta->size += byte_count;
               if (stack_meta->backtrace.frame_size == 0 && backtrace.frame_size != 0) {
                   stack_meta->backtrace = backtrace;
                   stack_meta->caller    = caller;
               }
            });
#endif

}

static inline void on_release_memory(void *ptr, bool is_mmap) {

    if (UNLIKELY(!ptr)) {
        LOGE(TAG, "on_release_memory: invalid pointer");
        return;
    }

    ON_MEMORY_RELEASED((uint64_t) ptr);

#if USE_MEMORY_MESSAGE_QUEUE == true
    buffer_queue_container_t * container = m_memory_messages_containers_.containers_[memory_ptr_hash((uintptr_t) ptr)];

    std::lock_guard<std::mutex> lock(container->mutex);
    if (!container->queue) {
        container->queue = new BufferQueue(SIZE_AUGMENT);
    }
    container->queue->enqueue_deletion_message(reinterpret_cast<uintptr_t>(ptr), is_mmap);
#else
    m_memory_meta_container.erase(ptr);
#endif
}

void on_alloc_memory(void *caller, void *ptr, size_t byte_count) {
    on_acquire_memory(caller, ptr, byte_count, false);
}

void on_free_memory(void *ptr) {
    on_release_memory(ptr, false);
}

void on_mmap_memory(void *caller, void *ptr, size_t byte_count) {
    on_acquire_memory(caller, ptr, byte_count, true);
}

void on_munmap_memory(void *ptr) {
    on_release_memory(ptr, true);
}

void memory_hook_on_dlopen(const char *file_name) {
    LOGD(TAG, "memory_hook_on_dlopen: file %s", file_name);
    // This line only refresh xhook in matrix-memoryhook library now.
    xhook_refresh(0);
}

// -------------------- Dump --------------------

/**
 * 区分 native heap 和 mmap 的 caller 和 stack
 * @param heap_caller_metas
 * @param mmap_caller_metas
 * @param heap_stack_metas
 * @param mmap_stack_metas
 */
static inline size_t collect_metas(std::map<void *, caller_meta_t> &heap_caller_metas,
                                   std::map<void *, caller_meta_t> &mmap_caller_metas,
                                   std::map<uint64_t, stack_meta_t> &heap_stack_metas,
                                   std::map<uint64_t, stack_meta_t> &mmap_stack_metas) {
    LOGD(TAG, "collect_metas");

    size_t ptr_meta_size = 0;

    m_memory_meta_container.for_each(
            [&](const void *ptr, ptr_meta_t *meta, stack_meta_t *stack_meta) {

                auto &dest_caller_metes =
                             meta->is_mmap ? mmap_caller_metas : heap_caller_metas;
                auto &dest_stack_metas  = meta->is_mmap ? mmap_stack_metas : heap_stack_metas;

                if (meta->caller) {
                    caller_meta_t &caller_meta = dest_caller_metes[meta->caller];
                    caller_meta.pointers.insert(ptr);
                    caller_meta.total_size += meta->size;
                }

                if (stack_meta) {
                    auto &dest_stack_meta = dest_stack_metas[meta->stack_hash];
                    dest_stack_meta.backtrace = stack_meta->backtrace;
                    // 没错, 这里的确使用 ptr_meta 的 size, 因为是在遍历 ptr_meta, 因此原来 stack_meta 的 size 仅起引用计数作用
                    dest_stack_meta.size += meta->size;
                    dest_stack_meta.caller    = stack_meta->caller;
                }

                ptr_meta_size++;
            });

    LOGD(TAG, "collect_metas done");
    return ptr_meta_size;
}

static inline void dump_callers(FILE *log_file,
                                cJSON *json_size_arr,
//                                const std::multimap<void *, ptr_meta_t> &ptr_metas,
                                std::map<void *, caller_meta_t> &caller_metas) {

    if (caller_metas.empty()) {
        LOGI(TAG, "dump_callers: nothing dump");
        return;
    }

    LOGD(TAG, "dump_callers: count = %zu", caller_metas.size());
    flogger0(log_file, "dump_callers: count = %zu\n", caller_metas.size());

    std::unordered_map<std::string, size_t>                   caller_alloc_size_of_so;
    std::unordered_map<std::string, std::map<size_t, size_t>> same_size_count_of_so;

    size_t ptr_counter = 0;

    LOGD(TAG, "caller so begin");
    // 按 so 聚类
    for (auto &i : caller_metas) {
        auto caller      = i.first;
        auto caller_meta = i.second;

        ptr_counter += caller_meta.pointers.size();

        Dl_info dl_info;
        dladdr(caller, &dl_info);

        if (!dl_info.dli_fname) {
            continue;
        }
        caller_alloc_size_of_so[dl_info.dli_fname] += caller_meta.total_size;

        // 按 size 聚类
        for (auto pointer : caller_meta.pointers) {
            m_memory_meta_container.get(pointer,
                                        [&same_size_count_of_so, &dl_info](ptr_meta_t &meta) {
                                            same_size_count_of_so[dl_info.dli_fname][meta.size]++;
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

    size_t caller_total_size = 0;

    for (auto i = result_sort_by_size.rbegin(); i != result_sort_by_size.rend(); ++i) {
        auto &so_name = i->second;
        auto &so_size = i->first;
        LOGD(TAG, "so = %s, caller alloc size = %zu", i->second.c_str(), i->first);
        cJSON *so_size_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(so_size_obj, "so", so_name.c_str());
        cJSON_AddStringToObject(so_size_obj, "size", std::to_string(so_size).c_str());
        cJSON_AddItemToArray(json_size_arr, so_size_obj);
        flogger0(log_file, "caller alloc size = %10zu b, so = %s\n", so_size, so_name.c_str());

        caller_total_size += so_size;

        auto count_of_size = same_size_count_of_so[so_name];

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
        flogger0(log_file, "top %d (size * count):\n", lines);

        for (auto sc = result_sort_by_mul.rbegin();
             sc != result_sort_by_mul.rend() && lines; ++sc, --lines) {
            auto size  = sc->second.first;
            auto count = sc->second.second;
            LOGD(TAG, "   size = %10zu b, count = %zu", size, count);
            flogger0(log_file, "   size = %10zu b, count = %zu\n", size, count);
        }
    }

    LOGD(TAG, "\n---------------------------------------------------");
    flogger0(log_file, "\n---------------------------------------------------\n");
    LOGD(TAG, "| caller total size = %zu b", caller_total_size);
    flogger0(log_file, "| caller total size = %zu b, ptr totals counts = %zu.\n", caller_total_size, ptr_counter);
    LOGD(TAG, "---------------------------------------------------\n");
    flogger0(log_file, "---------------------------------------------------\n\n");
}

struct stack_dump_meta_t {
    size_t      size;
    std::string full_stacktrace;
    std::string brief_stacktrace;
};

static inline void dump_stacks(FILE *log_file,
                               cJSON *json_mem_arr,
                               std::map<uint64_t, stack_meta_t> &stack_metas) {
    if (stack_metas.empty()) {
        LOGI(TAG, "stacktrace: nothing dump");
        return;
    }

    LOGD(TAG, "dump_stacks: hash count = %zu", stack_metas.size());
    flogger0(log_file, "dump_stacks: hash count = %zu\n", stack_metas.size());

    std::unordered_map<std::string, size_t>                         stack_alloc_size_of_so;
    std::unordered_map<std::string, std::vector<stack_dump_meta_t>> stacktrace_of_so;

    size_t backtrace_counter = 0;

    for (auto &stack_meta_it : stack_metas) {
        auto hash      = stack_meta_it.first;
        auto size      = stack_meta_it.second.size;
        auto backtrace = stack_meta_it.second.backtrace;
        auto caller    = stack_meta_it.second.caller;

        std::string       caller_so_name;
        std::stringstream full_stack_builder;
        std::stringstream brief_stack_builder;

        backtrace_counter += stack_meta_it.second.backtrace.frame_size;

        Dl_info caller_info{};
        dladdr(caller, &caller_info);

        if (caller_info.dli_fname != nullptr) {
            caller_so_name = caller_info.dli_fname;
        }

        std::string last_so_name; // 上一帧所属 so 名字

        auto _callback = [&](wechat_backtrace::FrameDetail it) {
            std::string so_name = it.map_name;

            char *demangled_name = nullptr;
            int  status          = 0;
            demangled_name = abi::__cxa_demangle(it.function_name, nullptr, 0, &status);

            full_stack_builder << "      | "
                               << "#pc " << std::hex << it.rel_pc << " "
                               << (demangled_name ? demangled_name : "(null)")
                               << " ("
                               << it.map_name
                               << ")"
                               << std::endl;

            if (last_so_name != it.map_name) {
                last_so_name = it.map_name;
                brief_stack_builder << it.map_name << ";";
            }

            brief_stack_builder << std::hex << it.rel_pc << ";";

            if (demangled_name) {
                free(demangled_name);
            }

            if (caller_so_name.empty()) { // fallback
                LOGD(TAG, "fallback getting so name -> caller = %p", stack_meta_it.second.caller);
                // fixme hard coding
                if (/*so_name.find("com.tencent.mm") == std::string::npos ||*/
                        so_name.find("libwechatbacktrace.so") != std::string::npos ||
                        so_name.find("libmatrix-hooks.so") != std::string::npos) {
                    return;
                }
                caller_so_name = so_name;
            }
        };

        wechat_backtrace::restore_frame_detail(backtrace.frames, backtrace.frame_size,
                                               _callback);

        stack_alloc_size_of_so[caller_so_name] += size;

        stack_dump_meta_t stack_dump_meta{size,
                                          full_stack_builder.str(),
                                          brief_stack_builder.str()};
        stacktrace_of_so[caller_so_name].emplace_back(stack_dump_meta);
    }

    LOGD(TAG, "dump_stacks: backtrace frame counts = %zu", backtrace_counter);
    flogger0(log_file, "dump_stacks: backtrace frame counts = %zu\n", backtrace_counter);

    // 从大到小排序
    std::vector<std::pair<std::string, size_t>> so_sorted_by_size;
    so_sorted_by_size.reserve(stack_alloc_size_of_so.size());
    for (auto &p : stack_alloc_size_of_so) {
        so_sorted_by_size.emplace_back(p);
    }
    std::sort(so_sorted_by_size.begin(), so_sorted_by_size.end(),
              [](const std::pair<std::string, size_t> &v1,
                 const std::pair<std::string, size_t> &v2) {
                  return v1.second > v2.second;
              });

    /*************************** prepared done ********************************/

    size_t json_so_count = 3;

    for (auto &p : so_sorted_by_size) {
        auto so_name       = p.first;
        auto so_alloc_size = p.second;

        LOGD(TAG, "\nmalloc size of so (%s) : remaining size = %zu", so_name.c_str(),
             so_alloc_size);
        flogger0(log_file, "\nmalloc size of so (%s) : remaining size = %zu\n", so_name.c_str(),
                so_alloc_size);

        if (so_alloc_size < m_stacktrace_log_threshold) {
            flogger0(log_file, "skip printing stacktrace for size less than %zu\n",
                    m_stacktrace_log_threshold);
            continue;
        }

        // 从大到小排序
        auto &stacktrace_sorted_by_size = stacktrace_of_so[so_name];
        std::sort(stacktrace_sorted_by_size.begin(), stacktrace_sorted_by_size.end(),
                  [](const stack_dump_meta_t &v1,
                     const stack_dump_meta_t &v2) {
                      return v1.size > v2.size;
                  });

        cJSON *so_obj       = nullptr; // nullable
        cJSON *so_stack_arr = nullptr; // nullable
        if (json_so_count) {
            LOGE(TAG
                         ".json", "json_so_count = %zu", json_so_count);
            json_so_count--;
            so_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(so_obj, "so", so_name.c_str());
            cJSON_AddStringToObject(so_obj, "size", std::to_string(so_alloc_size).c_str());
            so_stack_arr = cJSON_AddArrayToObject(so_obj, "top_stacks");
        }

        size_t json_stacktrace_count = 3;

        for (auto &stack_dump_meta : stacktrace_sorted_by_size) {

            LOGD(TAG, "malloc size of the same stack = %zu\n stacktrace : \n%s",
                 stack_dump_meta.size,
                 stack_dump_meta.full_stacktrace.c_str());

            flogger0(log_file, "malloc size of the same stack = %zu\n stacktrace : \n%s\n",
                    stack_dump_meta.size,
                    stack_dump_meta.full_stacktrace.c_str());

            if (json_so_count && json_stacktrace_count) {
                json_stacktrace_count--;
                LOGE(TAG
                             ".json", "json_stacktrace_count = %zu", json_stacktrace_count);
                cJSON *stack_obj = cJSON_CreateObject();
                cJSON_AddStringToObject(stack_obj, "size",
                                        std::to_string(stack_dump_meta.size).c_str());
                cJSON_AddStringToObject(stack_obj, "stack",
                                        stack_dump_meta.brief_stacktrace.c_str());
                cJSON_AddItemToArray(so_stack_arr, stack_obj);
            }
        }
        cJSON_AddItemToArray(json_mem_arr, so_obj);
    }
}

static inline void dump_impl(FILE *log_file, FILE *json_file, bool mmap) {

    std::map<void *, caller_meta_t>  heap_caller_metas;
    std::map<void *, caller_meta_t>  mmap_caller_metas;
    std::map<uint64_t, stack_meta_t> heap_stack_metas;
    std::map<uint64_t, stack_meta_t> mmap_stack_metas;

    size_t ptr_meta_size = collect_metas(heap_caller_metas,
                                         mmap_caller_metas,
                                         heap_stack_metas,
                                         mmap_stack_metas);

    cJSON *json_obj           = cJSON_CreateObject();
    cJSON *so_native_size_arr = cJSON_AddArrayToObject(json_obj, "SoNativeSize");

    // native heap allocation
    dump_callers(log_file, so_native_size_arr, heap_caller_metas);
    cJSON *native_heap_arr = cJSON_AddArrayToObject(json_obj, "NativeHeap");

    dump_stacks(log_file, native_heap_arr, heap_stack_metas);

    if (mmap) {
        // mmap allocation
        LOGD(TAG, "############################# mmap #############################\n\n");
        flogger0(log_file,
                "############################# mmap #############################\n\n");

        cJSON *so_mmap_size_arr = cJSON_AddArrayToObject(json_obj, "SoMmapSize");
        dump_callers(log_file, so_mmap_size_arr, mmap_caller_metas);
        cJSON *mmap_arr = cJSON_AddArrayToObject(json_obj, "mmap");
        dump_stacks(log_file, mmap_arr, mmap_stack_metas);
    }

    char *printed = cJSON_PrintUnformatted(json_obj);
    flogger0(json_file, "%s", printed);
    LOGD(TAG, "===> %s", printed);
    cJSON_free(printed);
    cJSON_Delete(json_obj);

    flogger0(log_file,
            "\n\n---------------------------------------------------\n"
            "<void *, ptr_meta_t> ptr_meta [%zu * %zu = (%zu)]\n"
            "<uint64_t, stack_meta_t> stack_meta [%zu * %zu = (%zu)]\n"
            "---------------------------------------------------\n",

            sizeof(ptr_meta_t) + sizeof(void *), ptr_meta_size,
            (sizeof(ptr_meta_t) + sizeof(void *)) * ptr_meta_size,

            sizeof(stack_meta_t) + sizeof(uint64_t),
            (heap_stack_metas.size() + mmap_stack_metas.size()),
            (sizeof(stack_meta_t) + sizeof(uint64_t)) *
            ((heap_stack_metas.size() + mmap_stack_metas.size())));

    LOGD(TAG,
         "<void *, ptr_meta_t> ptr_meta [%zu * %zu = (%zu)]\n"
         "<uint64_t, stack_meta_t> stack_meta [%zu * %zu = (%zu)]\n",

         sizeof(ptr_meta_t) + sizeof(void *), ptr_meta_size,
         (sizeof(ptr_meta_t) + sizeof(void *)) * ptr_meta_size,

         sizeof(stack_meta_t) + sizeof(uint64_t),
         (heap_stack_metas.size() + mmap_stack_metas.size()),
         (sizeof(stack_meta_t) + sizeof(uint64_t)) *
         ((heap_stack_metas.size() + mmap_stack_metas.size())));
}

void dump(bool enable_mmap, const char *log_path, const char *json_path) {
    LOGD(TAG,
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> memory dump begin <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");


    FILE *log_file  = log_path ? fopen(log_path, "w+") : nullptr;
    FILE *json_file = json_path ? fopen(json_path, "w+") : nullptr;
    LOGD(TAG, "dump path = %s", log_path);

    dump_impl(log_file, json_file, enable_mmap);

//    DUMP_RECORD("/sdcard/Android/data/com.tencent.mm/memory-record.dump");

    if (log_file) {
        fflush(log_file);
        fclose(log_file);
    }
    if (json_file) {
        fflush(json_file);
        fclose(json_file);
    }

    LOGD(TAG,
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> memory dump end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
}

// For testing.
EXPORT_C void fake_malloc(void * ptr, size_t byte_count) {
    void * caller = __builtin_return_address(0);
    on_alloc_memory(caller, ptr, byte_count);
}

// For testing.
EXPORT_C void fake_free(void * ptr) {
    on_free_memory(ptr);
}