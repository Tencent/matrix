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

#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/mman.h>
#include <libkern/OSAtomic.h>

#include "dyld_image_info.h"
#include "logger_internal.h"
#include "bundle_name_helper.h"

#pragma mark -
#pragma mark Defines

#pragma mark -
#pragma mark Types

#define DYLD_IMAGE_MACOUNT		(512+256)

struct dyld_image_info {
	uint64_t	vm_str;			/* the start address of this segment */
	uint64_t	vm_end;			/* the end address of this segment */
	char		uuid[33];		/* the 128-bit uuid */
	char		image_name[30];	/* name of shared object */
	bool		is_app_image;	/* whether this image is belong to the APP */
	
	dyld_image_info(uint64_t _vs=0, uint64_t _ve=0) : vm_str(_vs), vm_end(_ve) {}
	
	inline bool operator == (const dyld_image_info &another) const {
		return another.vm_str >= vm_str && another.vm_str < vm_end;
	}
	
	inline bool operator > (const dyld_image_info &another) const {
		return another.vm_str < vm_str;
	}
};

struct dyld_image_info_file {
	int	fd;
	int fs;
	int count;
	void *buff;
	dyld_image_info list[DYLD_IMAGE_MACOUNT];
};

#pragma mark -
#pragma mark Constants/Globals

static malloc_lock_s dyld_image_info_mutex; // init with 1
static dyld_image_info_file image_info_file;

int skip_max_stack_depth;
int skip_min_malloc_size;
bool is_ios9_plus = true;

static dyld_image_info app_image_info = {0}; // Infos of all app images including embeded frameworks
static dyld_image_info mmap_func_info = {0};

static const char *g_app_bundle_name = bundleHelperGetAppBundleName();
static const char *g_app_name = bundleHelperGetAppName();

#pragma mark -
#pragma mark DYLD

#ifdef __LP64__
typedef struct mach_header_64 mach_header_t;
typedef struct segment_command_64 segment_command_t;
typedef struct section_64 section_t;
typedef struct nlist_64 nlist_t;
#define LC_SEGMENT_ARCH_DEPENDENT LC_SEGMENT_64
#else
typedef struct mach_header mach_header_t;
typedef struct segment_command segment_command_t;
typedef struct section section_t;
typedef struct nlist nlist_t;
#define LC_SEGMENT_ARCH_DEPENDENT LC_SEGMENT
#endif

static void __save_to_file()
{
	if (image_info_file.fd < 0) {
		return;
	}
	
	memcpy(image_info_file.buff, &image_info_file, sizeof(dyld_image_info_file));
	msync(image_info_file.buff, image_info_file.fs, MS_SYNC);
}

static void __add_info_for_image(const struct mach_header *header, intptr_t slide)
{
	__malloc_lock_lock(&dyld_image_info_mutex);
	
	if (image_info_file.count >= DYLD_IMAGE_MACOUNT) {
		__malloc_lock_unlock(&dyld_image_info_mutex);
		return;
	}
	
	dyld_image_info image_info = {0};
	bool is_current_app_image = false;
    
	segment_command_t *cur_seg_cmd = NULL;
	uintptr_t cur = (uintptr_t)header + sizeof(mach_header_t);
	for (int i = 0; i < header->ncmds; ++i, cur += cur_seg_cmd->cmdsize) {
		cur_seg_cmd = (segment_command_t *)cur;
		if (cur_seg_cmd->cmd == LC_SEGMENT_ARCH_DEPENDENT) {
			if (strcmp(cur_seg_cmd->segname, SEG_TEXT) == 0) {
				image_info.vm_str = slide + cur_seg_cmd->vmaddr;
				image_info.vm_end = image_info.vm_str + cur_seg_cmd->vmsize;
			}
		} else if (cur_seg_cmd->cmd == LC_UUID) {
			const char hexStr[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
			uint8_t *uuid = ((struct uuid_command *)cur_seg_cmd)->uuid;
			
			for (int i = 0; i < 16; ++i) {
				image_info.uuid[i * 2] = hexStr[uuid[i] >> 4];
				image_info.uuid[i * 2 + 1] = hexStr[uuid[i] & 0xf];
			}
			image_info.uuid[32] = 0;

			Dl_info info = {0};
			if (dladdr(header, &info) != 0 && info.dli_fname) {
				if (strlen(info.dli_fname) > strlen(g_app_name) && !memcmp(info.dli_fname + strlen(info.dli_fname) - strlen(g_app_name), g_app_name, strlen(g_app_name))) {
                    is_current_app_image = true;
				}
				if (strrchr(info.dli_fname, '/') != NULL) {
					strncpy(image_info.image_name, strrchr(info.dli_fname, '/'), sizeof(image_info.image_name));
				}
        
				image_info.is_app_image = (strstr(info.dli_fname, g_app_bundle_name) != NULL);

			}
		}
	}
	
	// Sort list
	int i = 0;
	for (; i < image_info_file.count; ++i) {
		if (image_info_file.list[i] == image_info) {
			__malloc_lock_unlock(&dyld_image_info_mutex);
			return;
		} else if (image_info_file.list[i] > image_info) {
			for (int j = image_info_file.count - 1; j >= i; --j) {
				image_info_file.list[j + 1] = image_info_file.list[j];
			}
			break;
		}
	}
	image_info_file.list[i] = image_info;
	image_info_file.count++;
	
	if (image_info.is_app_image) {
		app_image_info.vm_str = (app_image_info.vm_str == 0 ? image_info.vm_str : MIN(app_image_info.vm_str, image_info.vm_str));
		app_image_info.vm_end = (app_image_info.vm_end == 0 ? image_info.vm_end : MAX(app_image_info.vm_end, image_info.vm_end));
		if (is_current_app_image) {
			memcpy(app_image_info.uuid, image_info.uuid, sizeof(image_info.uuid));
		}
	}
	
	__save_to_file();
	
	__malloc_lock_unlock(&dyld_image_info_mutex);
}

static void __dyld_image_add_callback(const struct mach_header *header, intptr_t slide)
{
	__add_info_for_image(header, slide);
}

static void __init_image_info_list()
{
	static bool static_isInit = false;
	
	if (!static_isInit) {
		static_isInit = true;
		
		image_info_file.fd = -1;
		image_info_file.buff = NULL;
		image_info_file.count = 0;
		
        if (!is_ios9_plus) {
            int images = _dyld_image_count();
            for (int i = 0; i < images; i++) {
                const struct mach_header *header = (mach_header *)_dyld_get_image_header(i);
                if (!header) continue;
                
                __add_info_for_image(header, _dyld_get_image_vmaddr_slide(i));
            }
        }
		
		dyld_image_info_mutex = __malloc_lock_init();
		_dyld_register_func_for_add_image(__dyld_image_add_callback);
	}
}

dyld_image_info *__binary_search(dyld_image_info_file *file, vm_address_t address)
{
	int low = 0;
	int high = file->count - 1;
	dyld_image_info *list = file->list;

	while (low <= high) {
		int midd = ((low + high) >> 1);
		if (list[midd].vm_str > address) {
			high = midd - 1;
		} else if (list[midd].vm_end < address) {
			low = midd + 1;
		} else {
			return list + midd;
		}
		
		midd = ((low + high) >> 1);
		if (list[midd].vm_str > address) {
			high = midd - 1;
		} else if (list[midd].vm_end < address) {
			low = midd + 1;
		} else {
			return list + midd;
		}
	}
	return NULL;
}

#pragma mark -
#pragma mark Public Interface

bool prepare_dyld_image_logger(const char *event_dir)
{
	__init_image_info_list();
	
	dyld_image_info_file *file = open_dyld_image_info_file(event_dir);
	if (file != NULL) {
		image_info_file.fd = file->fd;
		image_info_file.fs = file->fs;
		image_info_file.buff = file->buff;
		
		__malloc_lock_lock(&dyld_image_info_mutex);
		__save_to_file();
		__malloc_lock_unlock(&dyld_image_info_mutex);

		inter_free(file);
		
		return true;
	} else {
		return false;
	}
}

bool is_stack_frames_should_skip(uintptr_t *frames, int32_t count, uint64_t malloc_size, uint32_t type_flags)
{
	// skip allocation events from mapped_file
	if (type_flags & memory_logging_type_mapped_file_or_shared_mem) {
		if (mmap_func_info.vm_str == 0) {
			Dl_info info = {0};
			dladdr((const void *)frames[0], &info);
			if (strcmp(info.dli_sname, "mmap") == 0) {
				mmap_func_info.vm_str = frames[0];
			}
		}
		if (mmap_func_info.vm_str == frames[0]) {
			if (frames[1] >= app_image_info.vm_str && frames[1] < app_image_info.vm_end) {
				return false;
			} else {
				return true;
			}
		}
	}
	
	if (malloc_size >= skip_min_malloc_size) {
		return false;
	}
	
	// check whether there's any symbol not in this APP
	for (int i = MIN(count - 3, skip_max_stack_depth); i >= 1; --i) {
		if (frames[i] >= app_image_info.vm_str && frames[i] < app_image_info.vm_end) {
			return false;
		}
	}
	
	// skip this stack
	return true;
}

const char *app_uuid()
{
	__init_image_info_list();
	return app_image_info.uuid;
}

dyld_image_info_file *open_dyld_image_info_file(const char *event_dir)
{
	int fd = open_file(event_dir, "image_infos.dat");
	int fs = (int)round_page(sizeof(dyld_image_info_file));
	dyld_image_info_file *file = (dyld_image_info_file *)inter_malloc(sizeof(dyld_image_info_file));
	
	if (fd < 0) {
		err_code = MS_ERRC_DI_FILE_OPEN_FAIL;
		goto init_fail;
	} else {
		struct stat st = {0};
		if (fstat(fd, &st) == -1) {
			err_code = MS_ERRC_DI_FILE_SIZE_FAIL;
			goto init_fail;
		}
		if (st.st_size == 0 || st.st_size != fs) {
			// new file
			if (ftruncate(fd, fs) != 0) {
				err_code = MS_ERRC_DI_FILE_TRUNCATE_FAIL;
				goto init_fail;
			}

			void *buff = inter_mmap(NULL, fs, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
			if (buff == MAP_FAILED) {
				err_code = MS_ERRC_DI_FILE_MMAP_FAIL;
				goto init_fail;
			}
			
			memset(file, 0, sizeof(dyld_image_info_file));
			file->fd = fd;
			file->fs = fs;
			file->buff = buff;
		} else {
			void *buff = inter_mmap(NULL, fs, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
			if (buff == MAP_FAILED) {
				err_code = MS_ERRC_DI_FILE_MMAP_FAIL;
				goto init_fail;
			}
		
			memcpy(file, buff, sizeof(dyld_image_info_file));
			file->fd = fd;
			file->fs = fs;
			file->buff = buff;
			
			if (file->count < 0 || file->count > sizeof(file->list) / sizeof(dyld_image_info)) {
				// dirty data
				file->count = 0;
				memset(file->list, 0, sizeof(file->list));
			}
		}
	}
	
	return file;
	
init_fail:
	if (fd >= 0) close(fd);
	inter_free(file);
	return NULL;
}

void close_dyld_image_info_file(dyld_image_info_file *file_context)
{
	if (file_context == NULL) {
		return;
	}
	
	inter_munmap(file_context->buff, file_context->fs);
	close(file_context->fd);
	inter_free(file_context);
}

void transform_frames(dyld_image_info_file *file_context, uint64_t *src_frames, uint64_t *out_offsets, char const **out_uuids, char const **out_image_names, bool *out_is_app_images, int32_t count)
{
	dyld_image_info *last_info = NULL; // cache
	while (count--) {
		uint64_t address = src_frames[count];
		if (last_info && *last_info == address) {
			out_offsets[count] = address - last_info->vm_str;
			out_uuids[count] = last_info->uuid;
			out_image_names[count] = last_info->image_name;
			out_is_app_images[count] = last_info->is_app_image;
		} else {
			if ((last_info = __binary_search(file_context, (vm_address_t)address)) != NULL) {
				out_offsets[count] = address - last_info->vm_str;
				out_uuids[count] = last_info->uuid;
				out_image_names[count] = last_info->image_name;
				out_is_app_images[count] = last_info->is_app_image;
			} else {
				out_offsets[count] = 0;
				out_uuids[count] = "";
				out_image_names[count] = "";
				out_is_app_images[count] = false;
			}
		}
	}
}
