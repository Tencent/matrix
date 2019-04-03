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

#ifndef logger_internal_h
#define logger_internal_h

#include <mach/vm_statistics.h>
#include <errno.h>
#include <libkern/OSAtomic.h>
#include <os/lock.h>
#include <sys/_types/_off_t.h>
#include <mach/mach.h>

#include "memory_stat_err_code.h"

// Define this marco to enable VM Logger, but APP will be REJECTED by Appstore for PRIVATE API usage
#ifdef DEBUG
#define USE_PRIVATE_API
#endif

#define memory_logging_type_free            0
#define memory_logging_type_generic     	1	/* anything that is not allocation/deallocation */
#define memory_logging_type_alloc	        2	/* malloc, realloc, etc... */
#define memory_logging_type_dealloc	        4	/* free, realloc, etc... */
#define memory_logging_type_vm_allocate     16  /* vm_allocate or mmap */
#define memory_logging_type_vm_deallocate   32	/* vm_deallocate or munmap */
#define memory_logging_type_mapped_file_or_shared_mem	128

// The valid flags include those from VM_FLAGS_ALIAS_MASK, which give the user_tag of allocated VM regions.
#define memory_logging_valid_type_flags ( \
	memory_logging_type_generic | \
	memory_logging_type_alloc | \
	memory_logging_type_dealloc | \
	memory_logging_type_vm_allocate | \
	memory_logging_type_vm_deallocate | \
	memory_logging_type_mapped_file_or_shared_mem | \
	VM_FLAGS_ALIAS_MASK)

#define __FILE_NAME__ (strrchr(__FILE__,'/')+1)

#define __malloc_printf(FORMAT, ...) \
	do { \
		char msg[256] = {0}; \
		sprintf(msg, FORMAT, ##__VA_ARGS__); \
		log_internal(__FILE_NAME__, __LINE__, __FUNCTION__, msg); \
	} while(0)

extern int err_code;

// Lock Function
#if 1
typedef OSSpinLock malloc_lock_s;

__attribute__((always_inline))
static inline malloc_lock_s __malloc_lock_init() {
	return OS_SPINLOCK_INIT;
}

__attribute__((always_inline))
static inline void __malloc_lock_lock(malloc_lock_s *lock) {
	OSSpinLockLock(lock);
}

__attribute__((always_inline))
static inline void __malloc_lock_unlock(malloc_lock_s *lock) {
	OSSpinLockUnlock(lock);
}
#else
//typedef OSSpinLock malloc_lock_s;
typedef os_unfair_lock malloc_lock_s;

__attribute__((always_inline))
static inline malloc_lock_s __malloc_lock_init() {
	return OS_UNFAIR_LOCK_INIT;
}

__attribute__((always_inline))
static inline void __malloc_lock_lock(malloc_lock_s *lock) {
	os_unfair_lock_lock(lock);
}

__attribute__((always_inline))
static inline void __malloc_lock_unlock(malloc_lock_s *lock) {
	os_unfair_lock_unlock(lock);
}
#endif

//
typedef mach_port_t thread_id;

thread_id current_thread_id();
bool is_thread_ignoring_logging(thread_id tid);

// Allocation/Deallocation Function without Logging
void *inter_malloc(uint64_t memSize);
void *inter_realloc(void *oldMem, size_t newSize);
void inter_free(void *ptr);

void *inter_mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset);
int inter_munmap(void *start, size_t length);
	
// File Functions
int open_file(const char *dir_name, const char *file_name);
void remove_file(const char *dir_name, const char *file_name);
void remove_folder(const char *dir_name);

extern void disable_memory_logging(void);
extern void set_memory_logging_invalid(void);
extern void log_internal(const char *file, int line, const char *funcname, char *msg);
extern void log_internal_without_this_thread(thread_id tid);
extern void report_error(int error);

#endif /* logger_internal_h */
