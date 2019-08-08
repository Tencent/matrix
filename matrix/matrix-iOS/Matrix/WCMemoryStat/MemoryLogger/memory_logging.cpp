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

#include "logger_internal.h"
#include "memory_logging.h"
#include "object_event_handler.h"
#include "allocation_event_buffer.h"
#include "allocation_event_db.h"
#include "dyld_image_info.h"
#include "stack_frames_db.h"

#ifdef DEBUG
#define USE_PRIVATE_API
#endif

#pragma mark -
#pragma mark Constants/Globals

// single-thread access variables
static stack_frames_db *stack_frames_writer = NULL;
static allocation_event_db *allocation_event_writer = NULL;
static allocation_event_buffer *event_buffer = NULL;

// activation variables
static bool logging_is_enable = false; // set this to zero to stop logging memory ativities
static bool is_debug_tools_running = false;

// We set malloc_logger to NULL to disable logging, if we encounter errors
// during file writing
typedef void (malloc_logger_t)(uint32_t type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t result, uint32_t num_hot_frames_to_skip);

extern malloc_logger_t *malloc_logger;
// Private API, cannot use it directly!
static malloc_logger_t **syscall_logger;	// use this to set up syscall logging (e.g., vm_allocate, vm_deallocate, mmap, munmap)

static pthread_t working_thread = 0;
static thread_id working_thread_id = 0;
extern mach_port_t g_matrix_block_monitor_dumping_thread_id;

static int should_working_thread_lock = 0; // 1 is require for locking, 2 is should lock

// pre-declarations
void *__memory_logging_event_writing_thread(void *param);

#pragma mark -
#pragma mark Memory Logging

bool __prepare_working_thread()
{
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
	
	if (pthread_create(&working_thread, &tattr, __memory_logging_event_writing_thread, NULL) != KERN_SUCCESS) {
		return false;
	} else {
		return true;
	}
}

void __memory_event_callback(uint32_t type_flags, uintptr_t zone_ptr, uintptr_t arg2, uintptr_t arg3, uintptr_t return_val, uint32_t num_hot_to_skip)
{
    uintptr_t size = 0;
    uintptr_t ptr_arg = 0;
	bool is_alloc = false;
	
	if (!logging_is_enable) {
		return;
	}

	uint32_t alias = 0;
	VM_GET_FLAGS_ALIAS(type_flags, alias);
	// skip all VM allocation events from malloc_zone
	if (alias >= VM_MEMORY_MALLOC && alias <= VM_MEMORY_MALLOC_NANO) {
		return;
	}

    // check incoming data
    if (type_flags & memory_logging_type_alloc && type_flags & memory_logging_type_dealloc) {
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
    if (type_flags & memory_logging_type_dealloc || type_flags & memory_logging_type_vm_deallocate) {
        size = arg3;
		ptr_arg = arg2;
		if (ptr_arg == 0) {
			return; // free(nil)
		}
    }
    if (type_flags & memory_logging_type_alloc || type_flags & memory_logging_type_vm_allocate) {
		if (return_val == 0 || return_val == (uintptr_t)MAP_FAILED) {
			//return; // alloc that failed, but still record this allocation event
			return_val = 0;
		}
        size = arg2;
		is_alloc = true;
    }
    
	if (type_flags & memory_logging_type_vm_allocate || type_flags & memory_logging_type_vm_deallocate) {
		mach_port_t targetTask = (mach_port_t)zone_ptr;
		// For now, ignore "injections" of VM into other tasks.
		if (targetTask != mach_task_self()) {
			return;
		}
	}
    
    type_flags &= memory_logging_valid_type_flags;
	
	thread_id curr_thread = current_thread_id();
	
	if (curr_thread == working_thread_id || curr_thread == g_matrix_block_monitor_dumping_thread_id/* || is_thread_ignoring_logging(curr_thread)*/) {
        // Prevent a thread from deadlocking against itself if vm_allocate() or malloc()
        // is called below here, from woking thread or dumping thread
        return;
    }

	memory_logging_event curr_event;
	
	// gather stack, only alloc type
    if (is_alloc) {
		curr_event.stack_size = backtrace((void **)curr_event.stacks, STACK_LOGGING_MAX_STACK_SIZE);
		num_hot_to_skip += 1; // skip itself and caller
		if (curr_event.stack_size <= num_hot_to_skip) {
			// Oops!  Didn't get a valid backtrace from thread_stack_pcs().
			return;
		}

		if (is_stack_frames_should_skip(curr_event.stacks + num_hot_to_skip, curr_event.stack_size - num_hot_to_skip, size, type_flags)) {
			curr_event.stack_size = 0;
			// skip this event?
			return;
		} else {
			curr_event.num_hot_to_skip = num_hot_to_skip;
		}

		curr_event.address = return_val;
        curr_event.argument = (uint32_t)size;
		curr_event.event_type = EventType_Alloc;
		curr_event.type_flags = type_flags;
		curr_event.t_id = curr_thread;
    } else {
        curr_event.address = ptr_arg;
        curr_event.argument = (uint32_t)size;
		curr_event.event_type = EventType_Free;
		curr_event.type_flags = type_flags;
		curr_event.stack_size = 0;
    }
	
	append_event_to_buffer(event_buffer, &curr_event);
}

void __update_object_event(uint64_t address, uint32_t new_type)
{
    thread_id curr_thread = current_thread_id();
	if (curr_thread == working_thread_id || curr_thread == g_matrix_block_monitor_dumping_thread_id || !logging_is_enable) {
		return;
	}
	
	memory_logging_event curr_event;
	
	curr_event.address = address;
	curr_event.argument = new_type;
	curr_event.event_type = EventType_Update;
	curr_event.type_flags = memory_logging_type_generic;
	curr_event.stack_size = 0;
	
	append_event_to_buffer(event_buffer, &curr_event);
}

#pragma mark - Writing Process

void *__memory_logging_event_writing_thread(void *param)
{
	pthread_setname_np("Memory Logging");
	
	working_thread_id = current_thread_id(); // for preventing deadlock'ing on stack logging on a single thread
	log_internal_without_this_thread(working_thread_id);
	
	struct timeval delay;
	delay.tv_sec = 0;
	delay.tv_usec = 10 * 1000; // 10 ms
	
	while (logging_is_enable) {
		while (has_event_in_buffer(event_buffer) == false) {
			usleep(15000);
			//select(0, NULL, NULL, NULL, &delay);
		}
		
		if (!logging_is_enable) {
			break;
		}
		
		// pick an event from buffer
		int64_t next_index = 0;
		memory_logging_event *curr_event = get_event_from_buffer(event_buffer, &next_index);
		bool is_skip = (curr_event->event_type == EventType_Invalid);
		
		if (is_next_index_valid(event_buffer, next_index) == false) {
			// Impossible...
			continue;
		}
		
		// compaction
		uint32_t object_type = 0;
		if (curr_event->event_type == EventType_Alloc && has_event_in_buffer(event_buffer, next_index)) {
			memory_logging_event *next_event = get_event_from_buffer(event_buffer, NULL, next_index);
			if (curr_event->address == next_event->address) {
				if (curr_event->type_flags & memory_logging_type_alloc) {
					if (next_event->type_flags & memory_logging_type_dealloc) {
						// *waves hand* current allocation never occurred
						is_skip = true;
						next_event->event_type = EventType_Invalid;
					} else if (next_event->event_type == EventType_Update) {
						object_type = next_event->argument;
						next_event->event_type = EventType_Invalid;
					}
				} else if (next_event->type_flags & memory_logging_type_vm_deallocate) {
					// *waves hand* current allocation(VM) never occurred
					is_skip = true;
					next_event->event_type = EventType_Invalid;
				}
			}
		}
		
		if (!is_skip) {
			// Can't lock like this without brain, or affect performance
			//__malloc_lock_lock(&working_thread_lock);
			if (should_working_thread_lock == 1) {
				should_working_thread_lock = 2;
				while (should_working_thread_lock == 2);
			}
			
			if (curr_event->event_type == EventType_Alloc) {
				uint32_t stack_identifier = 0;
				if (curr_event->stack_size > 0) {
					stack_identifier = add_stack_frames_in_table(stack_frames_writer, curr_event->stacks + curr_event->num_hot_to_skip, curr_event->stack_size - curr_event->num_hot_to_skip); // unique stack in memory
				} else {
					log_internal_without_this_thread(0);
					
					__malloc_printf("Data corrupted!");
					
					//__malloc_lock_unlock(&working_thread_lock);
					// Restore abort()?
					//abort();
					disable_memory_logging();
					report_error(MS_ERRC_DATA_CORRUPTED);
					break;
				}
				// Try to get vm memory type from type_flags
				if (object_type == 0) {
					VM_GET_FLAGS_ALIAS(curr_event->type_flags, object_type);
				}
				add_allocation_event(allocation_event_writer, curr_event->address, curr_event->type_flags, object_type, curr_event->argument, stack_identifier, curr_event->t_id);
			} else if (curr_event->event_type == EventType_Free) {
				del_allocation_event(allocation_event_writer, curr_event->address, curr_event->type_flags);
			} else {
				update_allocation_event_object_type(allocation_event_writer, curr_event->address, curr_event->argument);
			}
			
			//__malloc_lock_unlock(&working_thread_lock);
		}
		
		update_read_index(event_buffer, next_index);
	}
	return NULL;
}

#pragma mark - Memory Stat Logging Thread

/*!
 @brief This function uses sysctl to check for attached debuggers.
 @link https://developer.apple.com/library/mac/qa/qa1361/_index.html
 @link http://www.coredump.gr/articles/ios-anti-debugging-protections-part-2/
 */
bool is_being_debugged(void)
{
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
	
	// Returns true if the current process is being debugged (either
	// running under the debugger or has a debugger attached post facto).
	int                 junk;
	int                 mib[4];
	struct kinfo_proc   info;
	size_t              size;
	
	// Initialize the flags so that, if sysctl fails for some bizarre
	// reason, we get a predictable result.
	
	info.kp_proc.p_flag = 0;
	
	// Initialize mib, which tells sysctl the info we want, in this case
	// we're looking for information about a specific process ID.
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = getpid();
	
	// Call sysctl.
	
	size = sizeof(info);
	junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
	assert(junk == 0);
	
	// We're being debugged if the P_TRACED flag is set.
	return ((info.kp_proc.p_flag & P_TRACED) != 0);
}

#pragma mark -
#pragma mark Public Interface

void flush_last_data(const char *log_dir)
{
	allocation_event_buffer *event_buffer = open_or_create_allocation_event_buffer(log_dir);
	if (event_buffer == NULL) {
		return;
	}
	
	if (has_event_in_buffer(event_buffer) == false) {
		__malloc_printf("has no event in buffer");
		close_allocation_event_buffer(event_buffer);
		return;
	}
	
	stack_frames_db *stack_frames_writer = open_or_create_stack_frames_db(log_dir);
	allocation_event_db *allocation_event_writer = open_or_create_allocation_event_db(log_dir);
	
	if (stack_frames_writer != NULL && allocation_event_writer != NULL) {
		while (has_event_in_buffer(event_buffer)) {
			int64_t next_index = 0;
			memory_logging_event *curr_event = get_event_from_buffer(event_buffer, &next_index);
			
			if (is_next_index_valid(event_buffer, next_index) == false) {
				__malloc_printf("next index not valid");
				break;
			}
			
			if (curr_event->event_type == EventType_Alloc) {
				uint32_t object_type = 0;
				uint32_t stack_identifier = 0;
				if (curr_event->stack_size > 0) {
					stack_identifier = add_stack_frames_in_table(stack_frames_writer, curr_event->stacks + curr_event->num_hot_to_skip, curr_event->stack_size - curr_event->num_hot_to_skip); // unique stack in memory
				} else {
					__malloc_printf("Data corrupted!");
					report_error(MS_ERRC_DATA_CORRUPTED2);
					break;
				}
				// Try to get vm memory type from type_flags
				if (object_type == 0) {
					VM_GET_FLAGS_ALIAS(curr_event->type_flags, object_type);
				}
				add_allocation_event(allocation_event_writer, curr_event->address, curr_event->type_flags, object_type, curr_event->argument, stack_identifier, curr_event->t_id);
			} else if (curr_event->event_type == EventType_Free) {
				del_allocation_event(allocation_event_writer, curr_event->address, curr_event->type_flags);
			} else if (curr_event->event_type == EventType_Update) {
				update_allocation_event_object_type(allocation_event_writer, curr_event->address, curr_event->argument);
			}
			
			update_read_index(event_buffer, next_index);
		}
	}
	
	__malloc_printf("done");

	close_stack_frames_db(stack_frames_writer);
	close_allocation_event_db(allocation_event_writer);
	close_allocation_event_buffer(event_buffer);
}

int enable_memory_logging(const char *log_dir)
{
	err_code = MS_ERRC_SUCCESS;
	
#ifdef USE_PRIVATE_API
	// stack_logging_enable_logging
	int *stack_logging_enable_logging = (int *)dlsym(RTLD_DEFAULT, "stack_logging_enable_logging");
	if (stack_logging_enable_logging != NULL && *stack_logging_enable_logging != 0) {
		is_debug_tools_running = true;
	}
#endif

    // Check whether there's any analysis tool process logging memory.
    if (is_debug_tools_running || is_being_debugged()) {
        return MS_ERRC_ANALYSIS_TOOL_RUNNING;
    }
	
	logging_is_enable = true;

	allocation_event_writer = open_or_create_allocation_event_db(log_dir);
	if (allocation_event_writer == NULL) {
		return err_code;
	}
	
	stack_frames_writer = open_or_create_stack_frames_db(log_dir);
	if (stack_frames_writer == NULL) {
		return err_code;
	}
	
	//event_buffer = open_or_create_allocation_event_buffer(log_dir);
	event_buffer = open_or_create_allocation_event_buffer_static();
	if (event_buffer == NULL) {
		return err_code;
	}
	
	if (__prepare_working_thread() == false) {
		__malloc_printf("create writing thread fail");
		return MS_ERRC_WORKING_THREAD_CREATE_FAIL;
	}
	
	if (!prepare_dyld_image_logger(log_dir)) {
		return err_code;
	}
	
	if (!prepare_object_event_logger(log_dir)) {
		return err_code;
	}
	
    malloc_logger = __memory_event_callback;
	
#ifdef USE_PRIVATE_API
    // __syscall_logger
	syscall_logger = (malloc_logger_t **)dlsym(RTLD_DEFAULT, "__syscall_logger");
	if (syscall_logger != NULL) {
		*syscall_logger = __memory_event_callback;
	}
#endif
	
	return MS_ERRC_SUCCESS;
}

/*
 * Since there a many errors that could cause stack logging to get disabled, this is a convenience method
 * for disabling any future logging in this process and for informing the user.
 */
void disable_memory_logging(void)
{
	if (!logging_is_enable) {
		return;
	}
	
	__malloc_printf("stack logging disabled due to previous errors.\n");
	
	logging_is_enable = false;
	
	disable_object_event_logger();
	malloc_logger = NULL;
#ifdef USE_PRIVATE_API
    if (syscall_logger != NULL) {
        *syscall_logger = NULL;
    }
#endif
	// avoid that after the memory monitoring stops, there are still some events being written.
	reset_write_index(event_buffer);
	// make current logging invalid
	set_memory_logging_invalid();
}

uint32_t get_current_thread_memory_usage()
{
	if (!logging_is_enable || !allocation_event_writer) {
		return 0;
	}
	
	__block uint32_t  total = 0;
	__block thread_id curr_thread = current_thread_id();
	
	should_working_thread_lock = 1;
	while (should_working_thread_lock != 2);
	
	enumerate_allocation_event(allocation_event_writer, ^(const allocation_event &event) {
		if (event.t_id == curr_thread) {
			total += event.size;
		}
	});
	
	should_working_thread_lock = 0;
	
	return total;
}
