/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
b * limitations under the License.
 */

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "ptrace-arch.h"
#include "libudf_unwind_p.h"
#include "ptrace.h"

uint32_t d15_get_word(void *addr) __attribute__((noinline));

uint32_t d15_get_word(void *addr) {
#if defined(__arm__)
  int val = 0xdeadaee0;
  __asm__ volatile ("ldr     %0, [%1, #0]\n"
		"b       1f\n"
		".long   0x75666475\n"
		".long   0x00006477\n"
		"1:\n"
		:"+r"(val):"r"(addr):"memory");
  return val;
#else
  return *(uint32_t *)addr;
#endif
}

#if 0
uint32_t d15_get_word_wo(void *addr) {
  return *(uint32_t *)addr;
}
#endif

void init_memory(memory_t* memory, const map_info_t* map_info_list) {
    memory->tid = -1;
    memory->map_info_list = map_info_list;
}

void init_memory_ptrace(memory_t* memory, pid_t tid) {
    memory->tid = tid;
    memory->map_info_list = NULL;
}

bool try_get_word(const memory_t* memory, uintptr_t ptr, uint32_t* out_value)
{
    LIBUDF_LOG("try_get_word: reading word at %p", (void*) ptr);
    if (ptr & 3) {
	LIBUDF_LOG("try_get_word: invalid pointer %p", (void*) ptr);
        *out_value = 0xffffffffL;
        return false;
    }
    if (memory->tid < 0) {
#if 0
        *out_value = *(uint32_t*)ptr;
        return true;
#else
        uint32_t out = d15_get_word((void *)ptr);
        if (out != 0xdeadaee0) {
          *out_value = out;
          return true;
        } else {
          *out_value = 0xffffffffL;
          return false;
        }
#endif
    } else {
        // ptrace() returns -1 and sets errno when the operation fails.
        // To disambiguate -1 from a valid result, we clear errno beforehand.
        errno = 0;
        *out_value = ptrace(PTRACE_PEEKTEXT, memory->tid, (void*)ptr, NULL);
        if (*out_value == 0xffffffffL && errno) {
            LIBUDF_LOG("try_get_word: invalid pointer 0x%08x reading from tid %d, "
                       "ptrace() errno=%d", ptr, memory->tid, errno);
            return false;
        }
        return true;
    }
}

static size_t _main_thread_stack_start = 0;
static size_t _main_thread_stack_size = 0;
static pid_t ubrd_pid;

static int ubrd_get_main_thread_stack(pid_t pid)
{
    char c, line[1024];
    int i = 0, j = 0, fd = -1;
    void* lib = NULL;
	int (*close_fptr)(int) = NULL; //use func ptr to avoid open/close rehook deadlock
	int (*open_fptr)(const char *, int, ...) = NULL;

	lib = dlopen("libc.so", RTLD_NOW|RTLD_GLOBAL);
	if (lib == NULL) {
		LIBUDF_LOG("Could not open libc.so: %s\n", dlerror());
		return -1;
	}

	close_fptr = (int (*)(int))dlsym(lib, "close");
	open_fptr = (int (*)(const char *, int, ...))dlsym(lib, "open");
	if (!close_fptr || !open_fptr) {
		LIBUDF_LOG("invalid open/close: %s\n", dlerror());
		dlclose(lib);
		return -1;
	}

	snprintf(line, sizeof(line), "/proc/self/task/%d/maps", pid);

	fd = open_fptr(line, O_RDONLY);
	if (fd < 0) {
		LIBUDF_LOG("/proc/%d/task/%d/maps open fail\n", pid, pid);
		dlclose(lib);
		return -1;
	}

	while (1) {
		if (!read(fd, &c, 1)) break;
		if (c == '\n') {
			line[i] = '\0';
			if (strncmp((const char *)&line[i-7], "[stack]", 7) == 0) {
				const char* stack_start;
				LIBUDF_LOG("stack:%s\n", line);
			#if defined(__LP64__)
				//64bit main stack format
				//7ff1bf1000-7ff1c12000 rw-p 00000000 00:00 0                              [stack]
				line[9] = '0';
				line[10] = 'x';
				line[21] = '\0'; //line[11~20] stack start
				stack_start = (const char*)&line[9];
			#else
				//32bit main stack format
				//becd7000-becf8000 rw-p 00000000 00:00 0          [stack]
				line[7] = '0';
				line[8] = 'x';
				line[17] = '\0'; //line[9~16] 32bit main thread stack start
				stack_start = (const char*)&line[7];
			#endif
				_main_thread_stack_start = strtoul(stack_start, (char **)NULL, 16);
				_main_thread_stack_size = 8 * 1024 * 1024; //bypass RLIMIT check for simple handling
				LIBUDF_LOG("[LCH_DEBUG]main_thread_stack_start:%p, main_thread_stack_size:%p\n",
					   (void *)_main_thread_stack_start, (void *)_main_thread_stack_size);
				close_fptr(fd);
				dlclose(lib);
				return 0;
			}
			else {
				i = 0;
			}
		}
		else {
			line[i++] = c;
		}
	}
    close_fptr(fd);

    // stack is 28th parameter from /proc/self/stat
    fd = open_fptr("/proc/self/stat", O_RDONLY);
    if (fd < 0) {
         LIBUDF_LOG("/proc/self/stat open fail\n");
        dlclose(lib);
        return -1;
    }

    i = j = 0;
    line[0] = '\0';
    while (1) {
        if (!read(fd, &c, 1)) break;
        if (c == ' ') {
            j++;
            if (j == 28) {
                line[i] = '\0';
                break;
            }
        }
        else if (j == 27) {
            line[i++] = c;
        }
    }

    if (line[0] != '\0') {
        _main_thread_stack_start =  strtoul(line, (char **)NULL, 10);
        _main_thread_stack_size = 8 * 1024 * 1024;  // bypass RLIMIT check for simple handling
        LIBUDF_LOG("[LCH_DEBUG]main_thread_stack_start:%p, main_thread_stack_size:%p\n",
                (void *)_main_thread_stack_start, (void *)_main_thread_stack_size);
        close_fptr(fd);
        dlclose(lib);
        return 0;
    }

	close_fptr(fd);
	dlclose(lib);
	return -1;
}

//return 0: success, -1 fail
static int ubrd_get_stack(size_t* pthread_stack_start, size_t* pthread_stack_end) {
	pid_t pid = getpid();

	if (pid != ubrd_pid) {
		LIBUDF_LOG("old pid:%d, new pid:%d\n", ubrd_pid, pid);
		ubrd_pid = pid;
		_main_thread_stack_start = 0; // reset for new process
		_main_thread_stack_size = 0;
	}

	if (gettid() == pid) {
		if (!_main_thread_stack_start) {
			if (ubrd_get_main_thread_stack(pid)) {
				LIBUDF_LOG("get_main_thread_stack fail\n");
				return -1;
			}
		}
		*pthread_stack_start = _main_thread_stack_start;
		*pthread_stack_end = _main_thread_stack_start - _main_thread_stack_size;
	}
	else {
		pthread_attr_t attr;
		pthread_getattr_np(pthread_self(), &attr);
		*pthread_stack_start = (size_t)attr.stack_base + attr.stack_size;
		*pthread_stack_end = (size_t)attr.stack_base;
	}
	return 0;
}

bool try_get_word_stack(uintptr_t ptr, uint32_t* out_value)
{
    size_t sstart = 0, send = 0;
    ubrd_get_stack(&sstart, &send);
    if ((ptr >= send) && (ptr <= sstart)) {
        *out_value = *(uint32_t*)ptr;
	return true;
    }
    return false;
}
