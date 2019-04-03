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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>

#include <pthread/pthread.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <malloc/malloc.h>

#include "logger_internal.h"

#pragma mark -
#pragma mark Constants/Globals

int err_code = MS_ERRC_SUCCESS;

static thread_id thread_ignore_logging = 0;
static malloc_lock_s shared_lock = __malloc_lock_init();
static malloc_zone_t *inter_zone = malloc_create_zone(getpagesize(), 0);

// Will lock these mapped memory while App is in background
static void  *mapped_mem[32] = {NULL};
static size_t mapped_size[32] = {0};
static bool app_is_in_backgound = false;

#pragma mark -
#pragma mark Thread ID

__attribute__((always_inline))
thread_id current_thread_id()
{
	return pthread_mach_thread_np(pthread_self());
}

void __set_thread_to_ignore_logging(thread_id tid, bool ignore)
{
	if (ignore) {
		__malloc_lock_lock(&shared_lock);
		thread_ignore_logging = tid;
	} else {
		thread_ignore_logging = 0;
		__malloc_lock_unlock(&shared_lock);
	}
}

__attribute__((always_inline))
bool is_thread_ignoring_logging(thread_id tid)
{
	return thread_ignore_logging == tid;
}

#pragma mark -
#pragma mark Allocation/Deallocation Function without Logging

void *inter_malloc(uint64_t memSize)
{
	__set_thread_to_ignore_logging(current_thread_id(), true);
	void *allocatedMem = inter_zone->malloc(inter_zone, (size_t)memSize);
	__set_thread_to_ignore_logging(current_thread_id(), false);
	return allocatedMem;
}

void *inter_realloc(void *oldMem, size_t newSize)
{
	__set_thread_to_ignore_logging(current_thread_id(), true);
	void *newMem = inter_zone->realloc(inter_zone, oldMem, newSize);
	__set_thread_to_ignore_logging(current_thread_id(), false);
	return newMem;
}

void inter_free(void *ptr)
{
	__set_thread_to_ignore_logging(current_thread_id(), true);
	inter_zone->free(inter_zone, ptr);
	__set_thread_to_ignore_logging(current_thread_id(), false);
}

void __add_mapped_mem(void *mem, size_t size)
{
	__malloc_lock_lock(&shared_lock);
	for (int i = 0; i < sizeof(mapped_mem) / sizeof(void *); ++i) {
		if (mapped_mem[i] == NULL) {
			mapped_mem[i] = mem;
			mapped_size[i] = size;
			break;
		}
	}
	if (app_is_in_backgound) {
		mlock(mem, size);
	}
	__malloc_lock_unlock(&shared_lock);
}

void __remove_mapped_mem(void *mem, size_t size)
{
	__malloc_lock_lock(&shared_lock);
	for (int i = 0; i < sizeof(mapped_mem) / sizeof(void *); ++i) {
		if (mapped_mem[i] == mem && mapped_size[i] == size) {
			mapped_mem[i] = NULL;
			mapped_size[i] = 0;
			break;
		}
	}
	if (app_is_in_backgound) {
		munlock(mem, size);
	}
	__malloc_lock_unlock(&shared_lock);
}

void *inter_mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset)
{
	__set_thread_to_ignore_logging(current_thread_id(), true);
	void *mappedMem = mmap(start, length, prot, flags, fd, offset);
	__set_thread_to_ignore_logging(current_thread_id(), false);

	__add_mapped_mem(mappedMem, length);
	
	return mappedMem;
}

int inter_munmap(void *start, size_t length)
{
	__remove_mapped_mem(start, length);
	
	__set_thread_to_ignore_logging(current_thread_id(), true);
	int ret = munmap(start, length);
	if (ret != 0) {
		__malloc_printf("munmap fail, %s, errno: %d", strerror(errno), errno);
		abort();
	}
	__set_thread_to_ignore_logging(current_thread_id(), false);
	
	return ret;
}

#pragma mark-
#pragma mark File Functions

int open_file(const char *dir_name, const char *file_name)
{
	char file_path[PATH_MAX] = {0};
	
	strlcat(file_path, dir_name, (size_t)PATH_MAX);
	
	// Add the '/' only if it's not already there.  Having multiple '/' characters works
	// but is unsightly when we print these stack log file names out.
	size_t file_path_len = strlen(file_path);
	if (file_path[file_path_len-1] != '/') {
		// use strlcat to null-terminate for the next strlcat call, and to check buffer size
		strlcat(file_path + file_path_len, "/", (size_t)PATH_MAX);
	}
	
	// Append the file name to file_path but don't use snprintf since
	// it can cause malloc() calls.
	strlcat(file_path, file_name, (size_t)PATH_MAX);
	
	// Securely create the log file.
	return open(file_path, O_RDWR | O_CREAT, S_IRWXU);
}

void remove_file(const char *dir_name, const char *file_name)
{
	char file_path[PATH_MAX] = {'\0'};
	
	strlcat(file_path, dir_name, (size_t)PATH_MAX);
	
	// Add the '/' only if it's not already there.  Having multiple '/' characters works
	// but is unsightly when we print these stack log file names out.
	size_t file_path_len = strlen(file_path);
	if (file_path[file_path_len-1] != '/') {
		// use strlcat to null-terminate for the next strlcat call, and to check buffer size
		strlcat(file_path + file_path_len, "/", (size_t)PATH_MAX);
	}
	
	// Append the file name to file_path but don't use snprintf since
	// it can cause malloc() calls.
	strlcat(file_path, file_name, (size_t)PATH_MAX);
	remove(file_path);
}

void remove_folder(const char *dir_name)
{
	DIR				*dp;
	struct dirent	*ep;
	char            p_buf[PATH_MAX] = {0};
	
	dp = opendir(dir_name);
	if (!dp) return;
	
	while ((ep = readdir(dp)) != NULL) {
		if (ep->d_namlen < 1) {
			continue;
		}
		if (ep->d_type == DT_DIR && ((ep->d_namlen == 1 && ep->d_name[0] == '.')
									 || (ep->d_namlen == 2 && ep->d_name[0] == '.' && ep->d_name[1] == '.'))) {
			// skip "." and ".."
			continue;
		}
		sprintf(p_buf, "%s/%s", dir_name, ep->d_name);
		if (ep->d_type == DT_DIR)
			remove_folder(p_buf);
		else
			unlink(p_buf);
	}
	
	closedir(dp);
	rmdir(dir_name);
}
