/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <dirent.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <paths.h>
#include <errno.h>
#include <assert.h>
#include <pthread/pthread.h>
#include <execinfo.h>
#include <mach-o/dyld.h>

#include <atomic>

#include "logger_internal.h"
#include "memory_logging.h"
#include "object_event_handler.h"
#include "memory_logging_event_buffer.h"
#include "memory_logging_event_buffer_list.h"
#include "memory_logging_event_buffer_pool.h"
#include "allocation_event_db.h"
#include "dyld_image_info.h"
#include "stack_frames_db.h"

#pragma mark -
#pragma mark Constants/Globals

// single-thread access variables
static stack_frames_db *s_stack_frames_writer = NULL;
static allocation_event_db *s_allocation_event_writer = NULL;
static dyld_image_info_db *s_dyld_image_info_writer = NULL;
static object_type_db *s_object_type_writer = NULL;
static memory_logging_event_buffer_list *s_buffer_list = NULL;
static memory_logging_event_buffer_pool *s_buffer_pool = NULL;

static pthread_key_t s_event_buffer_key = 0;

// activation variables
static bool s_logging_is_enable = false; // set this to zero to stop logging memory ativities

// We set malloc_logger to NULL to disable logging, if we encounter errors
// during file writing
typedef void(malloc_logger_t)(uint32_t type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t result, uint32_t num_hot_frames_to_skip);

extern malloc_logger_t *malloc_logger;
// Private API, cannot use it directly!
#ifdef USE_PRIVATE_API
static malloc_logger_t **syscall_logger; // use this to set up syscall logging (e.g., vm_allocate, vm_deallocate, mmap, munmap)
#endif

thread_id s_main_thread_id = 0;
static pthread_t s_working_thread = 0;
static thread_id s_working_thread_id = 0;
//extern thread_id g_matrix_block_monitor_dumping_thread_id;

static std::atomic_int s_working_thread_lock(0); // 1 is require for locking, 2 is should lock

// Some callback in memory logging thread
static void (*s_dump_memory_callback)(void *, void *, void *, void *, void *, void *) = NULL;

// pre-declarations
void *__memory_event_writing_thread(void *param);

#pragma mark -
#pragma mark Memory Logging

bool __prepare_working_thread() {
    int ret;
    pthread_attr_t tattr;
    sched_param param;

    /* initialized with default attributes */
    ret = pthread_attr_init(&tattr);

    /* safe to get existing scheduling param */
    ret = pthread_attr_getschedparam(&tattr, &param);

    /* set the highest priority; others are unchanged */
    param.sched_priority = MAX(sched_get_priority_max(SCHED_RR), param.sched_priority);

    /* setting the new scheduling param */
    ret = pthread_attr_setschedparam(&tattr, &param);

    if (pthread_create(&s_working_thread, &tattr, __memory_event_writing_thread, NULL) == KERN_SUCCESS) {
        pthread_detach(s_working_thread);
        return true;
    } else {
        return false;
    }
}

#if __has_feature(ptrauth_calls)
#include <ptrauth.h>
#endif
//#include <pthread/stack_np.h> // iOS 12+ only

__attribute__((noinline, not_tail_called)) static unsigned __thread_stack_pcs(uintptr_t *buffer, unsigned max, int skip) {
    uintptr_t frame, next;
    pthread_t self = pthread_self();
    uintptr_t stacktop = (uintptr_t)pthread_get_stackaddr_np(self);
    uintptr_t stackbot = stacktop - pthread_get_stacksize_np(self);
    unsigned nb = 0;

    // Rely on the fact that our caller has an empty stackframe (no local vars)
    // to determine the minimum size of a stackframe (frame ptr & return addr)
    frame = (uintptr_t)__builtin_frame_address(1);

#define INSTACK(a) ((a) >= stackbot && (a) < stacktop)

#if defined(__x86_64__)
#define ISALIGNED(a) ((((uintptr_t)(a)) & 0xf) == 0)
#elif defined(__i386__)
#define ISALIGNED(a) ((((uintptr_t)(a)) & 0xf) == 8)
#elif defined(__arm__) || defined(__arm64__)
#define ISALIGNED(a) ((((uintptr_t)(a)) & 0x1) == 0)
#endif

    if (!INSTACK(frame) || !ISALIGNED(frame)) {
        return 0;
    }

    // skip itself and caller
    while (skip--) {
        next = *(uintptr_t *)frame;
        frame = next;
        if (!INSTACK(frame) /* || !ISALIGNED(frame)*/) {
            return 0;
        }
    }

    while (max--) {
        next = *(uintptr_t *)frame;
        uintptr_t retaddr = *((uintptr_t *)frame + 1);
        //uintptr_t retaddr2 = 0;
        //uintptr_t next2 = pthread_stack_frame_decode_np(frame, &retaddr2);
        if (retaddr == 0) {
            return nb;
        }
#if __has_feature(ptrauth_calls)
        //buffer[nb++] = (uintptr_t)ptrauth_strip((void *)retaddr, ptrauth_key_return_address); // PAC strip
        buffer[nb++] = (retaddr & 0x0fffffffff); // PAC strip
#elif defined(__arm__) || defined(__arm64__)
        buffer[nb++] = (retaddr & 0x0fffffffff); // PAC strip
#else
        buffer[nb++] = retaddr;
#endif

        if (!INSTACK(next) /* || !ISALIGNED(next) || next <= frame*/) {
            return nb;
        }

        frame = next;
    }

#undef INSTACK
#undef ISALIGNED

    return nb;
}

memory_logging_event_buffer *__new_event_buffer_and_lock(thread_id t_id) {
    memory_logging_event_buffer *event_buffer = memory_logging_event_buffer_pool_new_buffer(s_buffer_pool);
    event_buffer->t_id = t_id;
    memory_logging_event_buffer_lock(event_buffer);
    memory_logging_event_buffer_list_push_back(s_buffer_list, event_buffer);
    pthread_setspecific(s_event_buffer_key, event_buffer);
    return event_buffer;
}

void __memory_event_callback(
uint32_t type_flags, uintptr_t zone_ptr, uintptr_t arg2, uintptr_t arg3, uintptr_t return_val, uint32_t num_hot_to_skip) {
    uintptr_t size = 0;
    uintptr_t ptr_arg = 0;
    bool is_alloc = false;

    if (!s_logging_is_enable) {
        return;
    }

    if (is_thread_ignoring_logging()) {
        // Prevent a thread from deadlocking against itself if vm_allocate() or malloc()
        // is called below here, from woking thread or dumping thread
        return;
    }

    uint32_t alias = 0;
    VM_GET_FLAGS_ALIAS(type_flags, alias);
    // skip all VM allocation events from malloc_zone
    if (alias >= VM_MEMORY_MALLOC && alias <= VM_MEMORY_MALLOC_NANO) {
        return;
    }

    // check incoming data
    if ((type_flags & memory_logging_type_alloc) && (type_flags & memory_logging_type_dealloc)) {
        size = arg3;
        ptr_arg = arg2; // the original pointer
        if (ptr_arg == return_val) {
            return; // realloc had no effect, skipping
        }
        if (ptr_arg == 0) { // realloc(NULL, size) same as malloc(size)
            type_flags ^= memory_logging_type_dealloc;
        } else {
            // realloc(arg1, arg2) -> result is same as free(arg1); malloc(arg2) -> result
            __memory_event_callback(memory_logging_type_dealloc, zone_ptr, ptr_arg, (uintptr_t)0, (uintptr_t)0, num_hot_to_skip + 1);
            __memory_event_callback(memory_logging_type_alloc, zone_ptr, size, (uintptr_t)0, return_val, num_hot_to_skip + 1);
            return;
        }
    }
    if ((type_flags & memory_logging_type_dealloc) || (type_flags & memory_logging_type_vm_deallocate)) {
        size = arg3;
        ptr_arg = arg2;
        if (ptr_arg == 0) {
            return; // free(nil)
        }
    }
    if ((type_flags & memory_logging_type_alloc) || (type_flags & memory_logging_type_vm_allocate)) {
        if (return_val == 0 || return_val == (uintptr_t)MAP_FAILED) {
            return;
        }
        size = arg2;
        is_alloc = true;
    }

    type_flags &= memory_logging_valid_type_flags;

    thread_id t_id = current_thread_id();
    memory_logging_event_buffer *event_buffer = (memory_logging_event_buffer *)pthread_getspecific(s_event_buffer_key);
    if (event_buffer == NULL || event_buffer->t_id != t_id) {
        event_buffer = __new_event_buffer_and_lock(t_id);
    } else {
        memory_logging_event_buffer_lock(event_buffer);
    }

    if (event_buffer->t_id != t_id) {
        memory_logging_event_buffer_unlock(event_buffer);

        event_buffer = __new_event_buffer_and_lock(t_id);
    }

    // gather stack, only alloc type
    if (is_alloc) {
        if (memory_logging_event_buffer_is_full(event_buffer, true)) {
            memory_logging_event_buffer_unlock(event_buffer);

            event_buffer = __new_event_buffer_and_lock(t_id);
        }

        memory_logging_event *new_event = memory_logging_event_buffer_new_event(event_buffer);
        new_event->stack_size = __thread_stack_pcs(new_event->stacks, STACK_LOGGING_MAX_STACK_SIZE, num_hot_to_skip);
        //new_event->stack_size = backtrace((void **)new_event->stacks, STACK_LOGGING_MAX_STACK_SIZE);
        new_event->address = return_val;
        new_event->size = (uint32_t)size;
        new_event->object_type = 0;
        new_event->t_id = t_id;
        new_event->type_flags = type_flags;
        new_event->event_size = (uint32_t)write_size_by_event(new_event);
        new_event->event_type = EventType_Alloc;

        memory_logging_event_buffer_update_write_index_with_size(event_buffer, new_event->event_size);
    } else {
        // compaction
        memory_logging_event *last_event = memory_logging_event_buffer_last_event(event_buffer);
        if (last_event != NULL && last_event->address == ptr_arg) {
            if ((last_event->type_flags & memory_logging_type_alloc) && (type_flags & memory_logging_type_dealloc)) {
                // skip events
                memory_logging_event_buffer_update_to_last_write_index(event_buffer);
                memory_logging_event_buffer_unlock(event_buffer);
                return;
            } else if ((last_event->type_flags & memory_logging_type_vm_allocate) && (type_flags & memory_logging_type_vm_deallocate)) {
                // skip events
                memory_logging_event_buffer_update_to_last_write_index(event_buffer);
                memory_logging_event_buffer_unlock(event_buffer);
                return;
            }
        }

        if (memory_logging_event_buffer_is_full(event_buffer)) {
            memory_logging_event_buffer_unlock(event_buffer);

            event_buffer = __new_event_buffer_and_lock(t_id);
        }

        memory_logging_event *new_event = memory_logging_event_buffer_new_event(event_buffer);
        new_event->address = ptr_arg;
        new_event->size = (uint32_t)size;
        new_event->type_flags = type_flags;
        new_event->event_size = MEMORY_LOGGING_EVENT_SIMPLE_SIZE;
        new_event->event_type = EventType_Free;

        memory_logging_event_buffer_update_write_index_with_size(event_buffer, new_event->event_size);
    }

    memory_logging_event_buffer_unlock(event_buffer);
}

void __memory_event_update_object(uint64_t address, uint32_t new_type) {
    if (!s_logging_is_enable) {
        return;
    }

    if (is_thread_ignoring_logging()) {
        return;
    }

    thread_id t_id = current_thread_id();
    memory_logging_event_buffer *event_buffer = (memory_logging_event_buffer *)pthread_getspecific(s_event_buffer_key);
    if (event_buffer == NULL || event_buffer->t_id != t_id) {
        event_buffer = __new_event_buffer_and_lock(t_id);
    } else {
        memory_logging_event_buffer_lock(event_buffer);
    }

    if (event_buffer->t_id != t_id) {
        memory_logging_event_buffer_unlock(event_buffer);

        event_buffer = __new_event_buffer_and_lock(t_id);
    }

    // compaction
    memory_logging_event *last_event = memory_logging_event_buffer_last_event(event_buffer);
    if (last_event != NULL && last_event->address == address) {
        if (last_event->type_flags & memory_logging_type_alloc) {
            // skip events
            last_event->object_type = new_type;
            memory_logging_event_buffer_unlock(event_buffer);
            return;
        }
    }

    if (memory_logging_event_buffer_is_full(event_buffer)) {
        memory_logging_event_buffer_unlock(event_buffer);

        event_buffer = __new_event_buffer_and_lock(t_id);
    }

    memory_logging_event *new_event = memory_logging_event_buffer_new_event(event_buffer);
    new_event->address = address;
    new_event->object_type = new_type;
    new_event->event_size = MEMORY_LOGGING_EVENT_SIMPLE_SIZE;
    new_event->event_type = EventType_Update;

    memory_logging_event_buffer_update_write_index_with_size(event_buffer, new_event->event_size);

    memory_logging_event_buffer_unlock(event_buffer);
}

#pragma mark - Writing Process

void *__memory_event_writing_thread(void *param) {
    pthread_setname_np("Memory Logging");

    set_curr_thread_ignore_logging(true); // for preventing deadlock'ing on memory logging on a single thread

    s_working_thread_id = current_thread_id();
    log_internal_without_this_thread(s_working_thread_id);

    // Wait for enable_memory_logging finished
    usleep(30000);

    while (s_logging_is_enable) {
        // Can't lock like this without brain, or affect performance
        if (s_working_thread_lock == 1) {
            s_working_thread_lock = 2;
            while (s_working_thread_lock == 2)
                ;
        }

        memory_logging_event_buffer *event_buffer = memory_logging_event_buffer_list_reset(s_buffer_list);
        while (event_buffer != NULL) {
            memory_logging_event_buffer_lock(event_buffer);
            event_buffer->t_id = 0;
            memory_logging_event_buffer_unlock(event_buffer);

            memory_logging_event_buffer_compress(event_buffer);
            memory_logging_event *curr_event = (memory_logging_event *)memory_logging_event_buffer_begin(event_buffer);

            while (curr_event != NULL) {
                if (curr_event->event_type == EventType_Alloc) {
                    if (is_stack_frames_should_skip(curr_event->stacks, curr_event->stack_size, curr_event->size, curr_event->type_flags) == false) {
                        uint32_t stack_identifier =
                        add_stack_frames_in_table(s_stack_frames_writer, curr_event->stacks, curr_event->stack_size); // unique stack in memory
                        // Try to get vm memory type from type_flags
                        uint32_t object_type = curr_event->object_type;
                        if (object_type == 0) {
                            VM_GET_FLAGS_ALIAS(curr_event->type_flags, object_type);
                        }
                        allocation_event_db_add(s_allocation_event_writer,
                                                curr_event->address,
                                                curr_event->type_flags,
                                                object_type,
                                                curr_event->size,
                                                stack_identifier,
                                                curr_event->t_id);
                    }
                } else if (curr_event->event_type == EventType_Free) {
                    allocation_event_db_del(s_allocation_event_writer, curr_event->address, curr_event->type_flags);
                } else if (curr_event->event_type == EventType_Update) {
                    allocation_event_db_update_object_type(s_allocation_event_writer, curr_event->address, curr_event->object_type);
                } else if (curr_event->event_type != EventType_Invalid) {
                    disable_memory_logging();
                    report_error(MS_ERRC_DATA_CORRUPTED);
                    __malloc_printf("Data corrupted?!");

                    return NULL;
                    // Restore abort()?
                }

                curr_event = memory_logging_event_buffer_next(event_buffer);
            }

            memory_logging_event_buffer *next_event_buffer = event_buffer->next_event_buffer;
            memory_logging_event_buffer_pool_free_buffer(s_buffer_pool, event_buffer);
            event_buffer = next_event_buffer;
        }

        if (s_dump_memory_callback) {
            s_dump_memory_callback(NULL, NULL, s_allocation_event_writer, s_stack_frames_writer, s_dyld_image_info_writer, s_object_type_writer);
            s_dump_memory_callback = NULL;
        }

        usleep(20000);
    }
    return NULL;
}

#pragma mark - Memory Stat Logging Thread

/*!
 @brief This function uses sysctl to check for attached debuggers.
 @link https://developer.apple.com/library/mac/qa/qa1361/_index.html
 @link http://www.coredump.gr/articles/ios-anti-debugging-protections-part-2/
 */
bool is_analysis_tool_running(void) {
    void *flagMallocStackLogging = getenv("MallocStackLogging");
    void *flagMallocStackLoggingNoCompact = getenv("MallocStackLoggingNoCompact");
    //flagMallocScribble = getenv("MallocScribble");
    //flagMallocGuardEdges = getenv("MallocGuardEdges");
    void *flagMallocLogFile = getenv("MallocLogFile");
    //flagMallocErrorAbort = getenv("MallocErrorAbort");
    //flagMallocCorruptionAbort = getenv("MallocCorruptionAbort");
    //flagMallocCheckHeapStart = getenv("MallocCheckHeapStart");
    //flagMallocHelp = getenv("MallocHelp");
    // Compatible with Instruments' Leak
    void *flagOAAllocationStatisticsOutputMask = getenv("OAAllocationStatisticsOutputMask");

    if (flagMallocStackLogging) {
        return true;
    }

    if (flagMallocStackLoggingNoCompact) {
        return true;
    }

    if (flagMallocLogFile) {
        return true;
    }

    if (flagOAAllocationStatisticsOutputMask) {
        return true;
    }

    return false;
}

#pragma mark -
#pragma mark Public Interface

int enable_memory_logging(const char *log_dir) {
    err_code = MS_ERRC_SUCCESS;

    // Check whether there's any analysis tool process logging memory.
    if (is_analysis_tool_running()) {
        return MS_ERRC_ANALYSIS_TOOL_RUNNING;
    }

    s_allocation_event_writer = allocation_event_db_open_or_create(log_dir);
    if (s_allocation_event_writer == NULL) {
        return err_code;
    }

    s_stack_frames_writer = stack_frames_db_open_or_create(log_dir);
    if (s_stack_frames_writer == NULL) {
        return err_code;
    }

    s_dyld_image_info_writer = prepare_dyld_image_logger(log_dir);
    if (s_dyld_image_info_writer == NULL) {
        return err_code;
    }

    s_object_type_writer = prepare_object_event_logger(log_dir);
    if (s_object_type_writer == NULL) {
        return err_code;
    }

    s_buffer_pool = memory_logging_event_buffer_pool_create();
    if (s_buffer_pool == NULL) {
        return err_code;
    }

    s_buffer_list = memory_logging_event_buffer_list_create();
    if (s_buffer_list == NULL) {
        return err_code;
    }

    if (__prepare_working_thread() == false) {
        __malloc_printf("create writing thread fail");
        return MS_ERRC_WORKING_THREAD_CREATE_FAIL;
    }

    pthread_key_create(&s_event_buffer_key, NULL);

    malloc_logger = __memory_event_callback;

#ifdef USE_PRIVATE_API
    // __syscall_logger
    syscall_logger = (malloc_logger_t **)dlsym(RTLD_DEFAULT, "__syscall_logger");
    if (syscall_logger != NULL) {
        *syscall_logger = __memory_event_callback;
    }
#endif

    if (pthread_main_np()) {
        s_main_thread_id = current_thread_id();
    } else {
        // memory logging shoule be enable in main thread
        abort();
    }
    logger_internal_init();
    s_logging_is_enable = true;

    return MS_ERRC_SUCCESS;
}

/*
 * Since there a many errors that could cause memory logging to get disabled, this is a convenience method
 * for disabling any future logging in this process and for informing the user.
 */
void disable_memory_logging(void) {
    if (!s_logging_is_enable) {
        return;
    }

    log_internal_without_this_thread(0);
    __malloc_printf("memory logging disabled due to previous errors.\n");

    s_logging_is_enable = false;

    disable_object_event_logger();
    malloc_logger = NULL;
#ifdef USE_PRIVATE_API
    if (syscall_logger != NULL) {
        *syscall_logger = NULL;
    }
#endif

    // avoid that after the memory monitoring stops, there are still some events being written.
    //memory_logging_event_buffer_pool_free(s_buffer_pool);
    //allocation_event_db_close(s_allocation_event_writer);
    //stack_frames_db_close(s_stack_frames_writer);
    // make current logging invalid
    set_memory_logging_invalid();
}

uint32_t get_current_thread_memory_usage() {
    if (!s_logging_is_enable || !s_allocation_event_writer) {
        return 0;
    }

    __block uint32_t total = 0;
    __block thread_id curr_thread = current_thread_id();

    s_working_thread_lock = 1;
    while (s_working_thread_lock != 2)
        ;

    allocation_event_db_enumerate(s_allocation_event_writer, ^(const uint64_t &address, const allocation_event &event) {
      if (event.t_id == curr_thread) {
          total += event.size;
      }
    });

    s_working_thread_lock = 0;

    return total;
}

bool dump_memory(void (*callback)(void *, void *, void *, void *, void *, void *)) {
    if (!s_logging_is_enable) {
        __malloc_printf("memory logging is disabled.\n");
        return false;
    }

    if (s_dump_memory_callback) {
        __malloc_printf("s_dump_memory_callback is not null.\n");
        return false;
    }

    s_dump_memory_callback = callback;
    return true;
}
