/*
 * Tencent is pleased to support the open source community by making
 * wechat-matrix available. Copyright (C) 2019 THL A29 Limited, a Tencent
 * company. All rights reserved. Licensed under the BSD 3-Clause License (the
 * "License"); you may not use this file except in compliance with the License.
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

#include <assert.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/nlist.h>
#include <pthread/pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include "bundle_name_helper.h"
#include "dyld_image_info.h"
#include "logger_internal.h"

#pragma mark -
#pragma mark Defines

#pragma mark -
#pragma mark Types

#define DYLD_IMAGE_MACOUNT (512 + 512)

struct dyld_image_info_mem {
    uint64_t vm_str; /* the start address of this segment */
    uint64_t vm_end; /* the end address of this segment */
    char uuid[33]; /* the 128-bit uuid */
    char image_name[30]; /* name of shared object */
    bool is_app_image; /* whether this image is belong to the APP */
    const struct mach_header *header;

    inline bool operator>(const dyld_image_info_mem &another) const { return another.vm_str < vm_str; }
};

struct dyld_image_info_db {
    int fd;
    int fs;
    int count;
    void *buff;
    malloc_lock_s lock;
    dyld_image_info_mem list[DYLD_IMAGE_MACOUNT];
};

#pragma mark -
#pragma mark Constants/Globals

static dyld_image_info_db *s_image_info_db = NULL;

int skip_max_stack_depth;
int skip_min_malloc_size;

static dyld_image_info_mem app_image_info = { 0 }; // Infos of all app images including embeded frameworks
static dyld_image_info_mem mmap_func_info = { 0 };

static char g_app_bundle_name[128];
static char g_app_name[128];

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

static void __save_to_file() {
    if (s_image_info_db->fd < 0) {
        return;
    }

    memcpy(s_image_info_db->buff, s_image_info_db, sizeof(dyld_image_info_db));
    msync(s_image_info_db->buff, s_image_info_db->fs, MS_SYNC);
}

static void __add_info_for_image(const struct mach_header *header, intptr_t slide) {
    __malloc_lock_lock(&s_image_info_db->lock);

    if (s_image_info_db->count >= DYLD_IMAGE_MACOUNT) {
        __malloc_lock_unlock(&s_image_info_db->lock);
        return;
    }

    dyld_image_info_mem image_info = { 0 };
    image_info.header = header;
    bool is_current_app_image = false;

    segment_command_t *cur_seg_cmd = NULL;
    segment_command_t *linkedit_segment = NULL;
    struct symtab_command *symtab_cmd = NULL;
    uintptr_t cur = (uintptr_t)header + sizeof(mach_header_t);
    for (int i = 0; i < header->ncmds; ++i, cur += cur_seg_cmd->cmdsize) {
        cur_seg_cmd = (segment_command_t *)cur;
        if (cur_seg_cmd->cmd == LC_SEGMENT_ARCH_DEPENDENT) {
            if (strcmp(cur_seg_cmd->segname, SEG_TEXT) == 0) {
                image_info.vm_str = slide + cur_seg_cmd->vmaddr;
                image_info.vm_end = image_info.vm_str + cur_seg_cmd->vmsize;
            } else if (strcmp(cur_seg_cmd->segname, SEG_LINKEDIT) == 0) {
                linkedit_segment = cur_seg_cmd;
            }
        } else if (cur_seg_cmd->cmd == LC_UUID) {
            const char hexStr[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
            uint8_t *uuid = ((struct uuid_command *)cur_seg_cmd)->uuid;

            for (int i = 0; i < 16; ++i) {
                image_info.uuid[i * 2] = hexStr[uuid[i] >> 4];
                image_info.uuid[i * 2 + 1] = hexStr[uuid[i] & 0xf];
            }
            image_info.uuid[32] = 0;

            Dl_info info = { 0 };
            if (dladdr(header, &info) != 0 && info.dli_fname) {
                if (strlen(info.dli_fname) > strlen(g_app_name)
                    && !memcmp(info.dli_fname + strlen(info.dli_fname) - strlen(g_app_name), g_app_name, strlen(g_app_name))) {
                    is_current_app_image = true;
                }
                if (strrchr(info.dli_fname, '/') != NULL) {
                    strncpy(image_info.image_name, strrchr(info.dli_fname, '/') + 1, sizeof(image_info.image_name));
                }

                image_info.is_app_image = (strstr(info.dli_fname, g_app_bundle_name) != NULL);
            }
        } else if (cur_seg_cmd->cmd == LC_SYMTAB) {
            symtab_cmd = (struct symtab_command *)cur_seg_cmd;
        }
    }

    // Sort list
    int i = 0;
    for (; i < s_image_info_db->count; ++i) {
        if (s_image_info_db->list[i].header == header) {
            s_image_info_db->list[i] = image_info;
            __malloc_lock_unlock(&s_image_info_db->lock);
            return;
        } else if (s_image_info_db->list[i] > image_info) {
            for (int j = s_image_info_db->count - 1; j >= i; --j) {
                s_image_info_db->list[j + 1] = s_image_info_db->list[j];
            }
            break;
        }
    }
    s_image_info_db->list[i] = image_info;
    s_image_info_db->count++;

    if (image_info.is_app_image) {
        app_image_info.vm_str = (app_image_info.vm_str == 0 ? image_info.vm_str : MIN(app_image_info.vm_str, image_info.vm_str));
        app_image_info.vm_end = (app_image_info.vm_end == 0 ? image_info.vm_end : MAX(app_image_info.vm_end, image_info.vm_end));
        if (is_current_app_image) {
            memcpy(app_image_info.uuid, image_info.uuid, sizeof(image_info.uuid));
        }
    }

    __save_to_file();

    __malloc_lock_unlock(&s_image_info_db->lock);
}

static void __remove_info_for_image(const struct mach_header *header, intptr_t slide) {
    __malloc_lock_lock(&s_image_info_db->lock);

    // Sort list
    int i = 0;
    for (; i < s_image_info_db->count; ++i) {
        if (s_image_info_db->list[i].header == header) {
            for (int j = i; j < s_image_info_db->count - 1; ++j) {
                s_image_info_db->list[j] = s_image_info_db->list[j + 1];
            }
            break;
        }
    }
    s_image_info_db->count--;

    __save_to_file();

    __malloc_lock_unlock(&s_image_info_db->lock);
}

static void __dyld_image_add_callback(const struct mach_header *header, intptr_t slide) {
    __add_info_for_image(header, slide);
}

static void __dyld_image_remove_callback(const struct mach_header *header, intptr_t slide) {
    __remove_info_for_image(header, slide);
}

static void __init_image_info_list() {
    if (s_image_info_db == NULL) {
        if (pthread_main_np() == 0) {
            // memory logging shoule be enable in main thread
            abort();
        }

        bundleHelperGetAppBundleName(g_app_bundle_name, sizeof(g_app_bundle_name));
        bundleHelperGetAppName(g_app_name, sizeof(g_app_name));
        // force load 'strstr' symbol
        strstr(g_app_name, g_app_bundle_name);

        s_image_info_db = (dyld_image_info_db *)inter_malloc(sizeof(dyld_image_info_db));

        s_image_info_db->fd = -1;
        s_image_info_db->buff = NULL;
        s_image_info_db->count = 0;
        s_image_info_db->lock = __malloc_lock_init();

        _dyld_register_func_for_add_image(__dyld_image_add_callback);
        _dyld_register_func_for_remove_image(__dyld_image_remove_callback);
    }
}

dyld_image_info_mem *__binary_search(dyld_image_info_db *file, vm_address_t address) {
    int low = 0;
    int high = file->count - 1;
    dyld_image_info_mem *list = file->list;

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

dyld_image_info_db *prepare_dyld_image_logger(const char *event_dir) {
    __init_image_info_list();

    dyld_image_info_db *db_context = dyld_image_info_db_open_or_create(event_dir);
    if (db_context != NULL) {
        s_image_info_db->fd = db_context->fd;
        s_image_info_db->fs = db_context->fs;
        s_image_info_db->buff = db_context->buff;

        __malloc_lock_lock(&s_image_info_db->lock);
        __save_to_file();
        __malloc_lock_unlock(&s_image_info_db->lock);

        inter_free(db_context);

        return s_image_info_db;
    } else {
        return NULL;
    }
}

#if __has_feature(ptrauth_calls)
#include <ptrauth.h>
#endif
//#include <pthread/stack_np.h> // iOS 12+ only

__attribute__((noinline, not_tail_called)) unsigned
thread_stack_pcs(pthread_stack_info *stack_info, uintptr_t *buffer, unsigned max, int skip, bool should_check, uint64_t *out_hash) {
    uintptr_t frame, next;
    pthread_t self;
    uintptr_t stacktop;
    uintptr_t stackbot;
    unsigned nb = 0;
    unsigned check_depth = (should_check ? skip_max_stack_depth : 0);
    size_t seed = 131; // 31 131 1313 13131 131313 etc..
    uint64_t hash = 0;

    if (stack_info) {
        self = stack_info->p_self;
        stacktop = stack_info->stacktop;
        stackbot = stack_info->stackbot;
    } else {
        self = pthread_self();
        stacktop = (uintptr_t)pthread_get_stackaddr_np(self);
        stackbot = stacktop - pthread_get_stacksize_np(self);
    }

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
        goto fail;
    }

    // skip itself and caller
    while (skip--) {
        next = *(uintptr_t *)frame;
        frame = next;
        if (!INSTACK(frame) /* || !ISALIGNED(frame)*/) {
            goto fail;
        }
    }

    while (max--) {
        next = *(uintptr_t *)frame;
        uintptr_t retaddr = *((uintptr_t *)frame + 1);
        // uintptr_t retaddr2 = 0;
        // uintptr_t next2 = pthread_stack_frame_decode_np(frame, &retaddr2);
        if (retaddr == 0) {
            goto success;
        }

        if (check_depth > 0 && nb >= 1) {
            check_depth--;
            if (retaddr >= app_image_info.vm_str && retaddr < app_image_info.vm_end) {
                check_depth = 0;
            } else if (check_depth == 0) {
                goto fail;
            }
        }

#if __has_feature(ptrauth_calls)
        // buffer[nb++] = (uintptr_t)ptrauth_strip((void *)retaddr,
        // ptrauth_key_return_address); // PAC strip
        buffer[nb++] = (retaddr & 0x0fffffffff); // PAC strip
#elif defined(__arm__) || defined(__arm64__)
        buffer[nb++] = (retaddr & 0x0fffffffff); // PAC strip
#else
        buffer[nb++] = retaddr;
#endif

        hash = hash * seed + retaddr;

        if (!INSTACK(next) /* || !ISALIGNED(next) || next <= frame*/) {
            goto success;
        }

        frame = next;
    }

#undef INSTACK
#undef ISALIGNED

success:
    if (check_depth > 0) {
        goto fail;
    }
    if (out_hash) {
        //*out_hash = ((hash << 6) | nb);
        *out_hash = hash;
    }
    return nb;

fail:
    *out_hash = 0;
    return 0;
}

const char *app_uuid() {
    __init_image_info_list();
    return app_image_info.uuid;
}

dyld_image_info_db *dyld_image_info_db_open_or_create(const char *event_dir) {
    int fd = open_file(event_dir, "image_infos.dat");
    int fs = (int)round_page(sizeof(dyld_image_info_db));
    dyld_image_info_db *db_context = (dyld_image_info_db *)inter_malloc(sizeof(dyld_image_info_db));

    if (fd < 0) {
        err_code = MS_ERRC_DI_FILE_OPEN_FAIL;
        goto init_fail;
    } else {
        struct stat st = { 0 };
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

            memset(db_context, 0, sizeof(dyld_image_info_db));
            db_context->fd = fd;
            db_context->fs = fs;
            db_context->buff = buff;
        } else {
            void *buff = inter_mmap(NULL, fs, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (buff == MAP_FAILED) {
                err_code = MS_ERRC_DI_FILE_MMAP_FAIL;
                goto init_fail;
            }

            memcpy(db_context, buff, sizeof(dyld_image_info_db));
            db_context->fd = fd;
            db_context->fs = fs;
            db_context->buff = buff;

            if (db_context->count < 0 || db_context->count > sizeof(db_context->list) / sizeof(dyld_image_info_mem)) {
                // dirty data
                db_context->count = 0;
                memset(db_context->list, 0, sizeof(db_context->list));
            }
        }
    }
    db_context->lock = __malloc_lock_init();

    return db_context;

init_fail:
    if (fd >= 0)
        close(fd);
    inter_free(db_context);
    return NULL;
}

void dyld_image_info_db_close(dyld_image_info_db *db_context) {
    if (db_context == NULL) {
        return;
    }

    if (db_context->fd >= 0) {
        close(db_context->fd);
        inter_munmap(db_context->buff, db_context->fs);
    }

    if (db_context == s_image_info_db) {
        // should not free s_image_info_db
        s_image_info_db->fd = -1;
        s_image_info_db->buff = NULL;
    } else {
        inter_free(db_context);
    }
}

void dyld_image_info_db_transform_frames(dyld_image_info_db *db_context,
                                         uint64_t *src_frames,
                                         uint64_t *out_offsets,
                                         char const **out_uuids,
                                         char const **out_image_names,
                                         bool *out_is_app_images,
                                         int32_t count) {
    dyld_image_info_mem *last_info = NULL; // cache
    while (count--) {
        uint64_t address = src_frames[count];
        if (last_info && last_info->vm_str <= address && address < last_info->vm_end) {
            out_offsets[count] = address - last_info->vm_str;
            out_uuids[count] = last_info->uuid;
            out_image_names[count] = last_info->image_name;
            out_is_app_images[count] = last_info->is_app_image;
        } else {
            if ((last_info = __binary_search(db_context, (vm_address_t)address)) != NULL) {
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
