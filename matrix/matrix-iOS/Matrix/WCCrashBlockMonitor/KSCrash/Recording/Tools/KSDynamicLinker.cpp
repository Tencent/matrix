//
//  KSDynamicLinker.c
//
//  Created by Karl Stenerud on 2013-10-02.
//
//  Copyright (c) 2012 Karl Stenerud. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall remain in place
// in this source code.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "KSDynamicLinker.h"
#import "KSCrash_BinaryImageHandler.h"

#include <limits.h>
#include <mach-o/dyld.h>
#include <mach-o/nlist.h>
#include <string.h>

#include "KSLogger.h"

#include <vector>
#include <unordered_map>

#include <assert.h>

#ifdef __LP64__
#define STRUCT_NLIST struct nlist_64
#else
#define STRUCT_NLIST struct nlist
#endif

bool s_enableLocalSymbolicate = false;

/** Get the image index that the specified address is part of.
 *
 * @param address The address to examine.
 * @return The index of the image it is part of, or UINT_MAX if none was found.
 */
static uint32_t imageIndexContainingAddress(const uintptr_t address) {
    const uint32_t imageCount = __ks_dyld_image_count();
    const struct mach_header *header = 0;

    for (uint32_t iImg = 0; iImg < imageCount; iImg++) {
        header = __ks_dyld_get_image_header(iImg);
        if (header != NULL) {
            // Look for a segment command with this address within its range.
            uintptr_t addressWSlide = address - (uintptr_t)__ks_dyld_get_image_vmaddr_slide(iImg);
            uintptr_t cmdPtr = __ks_first_cmd_after_header(header);
            if (cmdPtr == 0) {
                continue;
            }
            for (uint32_t iCmd = 0; iCmd < header->ncmds; iCmd++) {
                const struct load_command *loadCmd = (struct load_command *)cmdPtr;
                if (loadCmd->cmd == LC_SEGMENT) {
                    const struct segment_command *segCmd = (struct segment_command *)cmdPtr;
                    if (addressWSlide >= segCmd->vmaddr && addressWSlide < segCmd->vmaddr + segCmd->vmsize) {
                        return iImg;
                    }
                } else if (loadCmd->cmd == LC_SEGMENT_64) {
                    const struct segment_command_64 *segCmd = (struct segment_command_64 *)cmdPtr;
                    if (addressWSlide >= segCmd->vmaddr && addressWSlide < segCmd->vmaddr + segCmd->vmsize) {
                        return iImg;
                    }
                }
                cmdPtr += loadCmd->cmdsize;
            }
        }
    }
    return UINT_MAX;
}

/** Get the segment base address of the specified image.
 *
 * This is required for any symtab command offsets.
 *
 * @param idx The image index.
 * @return The image's base address, or 0 if none was found.
 */
static uintptr_t segmentBaseOfImageIndex(const uint32_t idx) {
    const struct mach_header *header = __ks_dyld_get_image_header(idx);
    if (header == NULL) {
        return 0;
    }

    // Look for a segment command and return the file image address.
    uintptr_t cmdPtr = __ks_first_cmd_after_header(header);
    if (cmdPtr == 0) {
        return 0;
    }
    for (uint32_t i = 0; i < header->ncmds; i++) {
        const struct load_command *loadCmd = (struct load_command *)cmdPtr;
        if (loadCmd->cmd == LC_SEGMENT) {
            const struct segment_command *segmentCmd = (struct segment_command *)cmdPtr;
            if (strcmp(segmentCmd->segname, SEG_LINKEDIT) == 0) {
                return segmentCmd->vmaddr - segmentCmd->fileoff;
            }
        } else if (loadCmd->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *segmentCmd = (struct segment_command_64 *)cmdPtr;
            if (strcmp(segmentCmd->segname, SEG_LINKEDIT) == 0) {
                return (uintptr_t)(segmentCmd->vmaddr - segmentCmd->fileoff);
            }
        }
        cmdPtr += loadCmd->cmdsize;
    }

    return 0;
}

uint32_t ksdl_imageNamed(const char *const imageName, bool exactMatch) {
    if (imageName != NULL) {
        const uint32_t imageCount = __ks_dyld_image_count();

        for (uint32_t iImg = 0; iImg < imageCount; iImg++) {
            const char *name = __ks_dyld_get_image_name(iImg);
            if (exactMatch) {
                if (strcmp(name, imageName) == 0) {
                    return iImg;
                }
            } else {
                if (strstr(name, imageName) != NULL) {
                    return iImg;
                }
            }
        }
    }
    return UINT32_MAX;
}

const uint8_t *ksdl_imageUUID(const char *const imageName, bool exactMatch) {
    if (imageName != NULL) {
        const uint32_t iImg = ksdl_imageNamed(imageName, exactMatch);
        if (iImg != UINT32_MAX) {
            const struct mach_header *header = __ks_dyld_get_image_header(iImg);
            if (header != NULL) {
                uintptr_t cmdPtr = __ks_first_cmd_after_header(header);
                if (cmdPtr != 0) {
                    for (uint32_t iCmd = 0; iCmd < header->ncmds; iCmd++) {
                        const struct load_command *loadCmd = (struct load_command *)cmdPtr;
                        if (loadCmd->cmd == LC_UUID) {
                            struct uuid_command *uuidCmd = (struct uuid_command *)cmdPtr;
                            return uuidCmd->uuid;
                        }
                        cmdPtr += loadCmd->cmdsize;
                    }
                }
            }
        }
    }
    return NULL;
}

bool ksdl_dladdr(const uintptr_t address, Dl_info *const info) {
    if (!s_enableLocalSymbolicate) {
        return false;
    }
    
    info->dli_fname = NULL;
    info->dli_fbase = NULL;
    info->dli_sname = NULL;
    info->dli_saddr = NULL;

    const uint32_t idx = imageIndexContainingAddress(address);
    if (idx == UINT_MAX) {
        return false;
    }
    const struct mach_header *header = __ks_dyld_get_image_header(idx);
    if (header == NULL) {
        return false;
    }
    const uintptr_t imageVMAddrSlide = (uintptr_t)__ks_dyld_get_image_vmaddr_slide(idx);
    const uintptr_t addressWithSlide = address - imageVMAddrSlide;
    const uintptr_t segmentBase = segmentBaseOfImageIndex(idx) + imageVMAddrSlide;
    if (segmentBase == 0) {
        return false;
    }

    const char *name = __ks_dyld_get_image_name(idx);
    if (name == NULL) {
        return false;
    }
    info->dli_fname = name;
    info->dli_fbase = (void *)header;

    // Find symbol tables and get whichever symbol is closest to the address.
    const STRUCT_NLIST *bestMatch = NULL;
    uintptr_t bestDistance = ULONG_MAX;
    uintptr_t cmdPtr = __ks_first_cmd_after_header(header);
    if (cmdPtr == 0) {
        return false;
    }
    for (uint32_t iCmd = 0; iCmd < header->ncmds; iCmd++) {
        const struct load_command *loadCmd = (struct load_command *)cmdPtr;
        if (loadCmd->cmd == LC_SYMTAB) {
            const struct symtab_command *symtabCmd = (struct symtab_command *)cmdPtr;
            //const STRUCT_NLIST* symbolTable = (STRUCT_NLIST*)(segmentBase + symtabCmd->symoff);
            const uintptr_t stringTable = segmentBase + symtabCmd->stroff;
            STRUCT_NLIST *next = (STRUCT_NLIST *)(segmentBase + symtabCmd->symoff);

            for (uint32_t iSym = 0; iSym < symtabCmd->nsyms; ++iSym, ++next) {
                // If n_value is 0, the symbol refers to an external object.
                uintptr_t symbolBase = next->n_value;

                if (symbolBase != 0 && addressWithSlide >= symbolBase) {
                    uintptr_t currentDistance = addressWithSlide - symbolBase;
                    if (currentDistance <= bestDistance) {
                        bestMatch = next;
                        bestDistance = currentDistance;
                    }
                }
            }
            if (bestMatch != NULL) {
                info->dli_saddr = (void *)(bestMatch->n_value + imageVMAddrSlide);
                if (bestMatch->n_desc == 16) {
                    // This image has been stripped. The name is meaningless, and
                    // almost certainly resolves to "_mh_execute_header"
                    info->dli_sname = NULL;
                } else {
                    info->dli_sname = (char *)((intptr_t)stringTable + (intptr_t)bestMatch->n_un.n_strx);
                    if (*info->dli_sname == '_') {
                        info->dli_sname++;
                    }
                }
                break;
            }
        }
        cmdPtr += loadCmd->cmdsize;
    }

    return true;
}

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

union symbol_info {
    uint64_t value;

    struct {
        uint64_t n_value : 36;
        uint64_t n_strx : 28;
    } detail;
};

struct image_symbol_infos {
    std::vector<symbol_info> *symbol_list;
    uintptr_t string_table;
    uintptr_t vm_str;
    uintptr_t vm_end;
    uint32_t image_index;
};

bool STRUCT_NLIST_compare(const symbol_info &a, const symbol_info &b) {
    return a.detail.n_value < b.detail.n_value;
}

image_symbol_infos *ksdl_build_symbol_list(const struct mach_header *header, uintptr_t segmentBase) {
    // Find symbol tables and get whichever symbol is closest to the address.
    uintptr_t cmdPtr = __ks_first_cmd_after_header(header);

    if (cmdPtr == 0) {
        return NULL;
    }

    image_symbol_infos *image_symbol = new image_symbol_infos();
    image_symbol->symbol_list = new std::vector<symbol_info>();

    bool found = false;

    for (uint32_t iCmd = 0; iCmd < header->ncmds; iCmd++) {
        const struct load_command *loadCmd = (struct load_command *)cmdPtr;

        if (loadCmd->cmd == LC_SEGMENT_ARCH_DEPENDENT) {
            const segment_command_t *segCmd = (segment_command_t *)cmdPtr;
            if (strcmp(segCmd->segname, SEG_TEXT) == 0) {
                image_symbol->vm_str = segCmd->vmaddr;
                image_symbol->vm_end = segCmd->vmaddr + segCmd->vmsize;
            }
        } else if (loadCmd->cmd == LC_SYMTAB) {
            assert(found == false);
            found = true;

            const struct symtab_command *symtabCmd = (struct symtab_command *)cmdPtr;
            image_symbol->string_table = segmentBase + symtabCmd->stroff;
            STRUCT_NLIST *next = (STRUCT_NLIST *)(segmentBase + symtabCmd->symoff);

            for (uint32_t iSym = 0; iSym < symtabCmd->nsyms; ++iSym, ++next) {
                // If n_value is 0, the symbol refers to an external object.
                uintptr_t symbolBase = next->n_value;

                if (symbolBase != 0) {
                    bool findSymbol = false;
                    // 代表这是一个调试符号
                    if (next->n_type & N_STAB) {
#define N_FNAME 0x22 /* procedure name (f77 kludge): name,,NO_SECT,0,0 */
#define N_FUN 0x24 /* procedure: name,,n_sect,linenumber,address */
#define N_STSYM 0x26 /* static symbol: name,,n_sect,type,address */
                        //if (next->n_type == N_FUN || next->n_type == N_STSYM) {
                        if (next->n_type == N_FNAME) {
                            findSymbol = true;
                        }
                    } else if ((next->n_type & N_TYPE) == N_SECT) {
                        findSymbol = true;
                    }

                    if (findSymbol) {
                        char *symbolName = (char *)((intptr_t)image_symbol->string_table + (intptr_t)next->n_un.n_strx);
                        if (*symbolName != 0 && *(symbolName + 1) != 0) {
                            symbol_info info = { .detail = { .n_value = next->n_value, .n_strx = next->n_un.n_strx } };
                            image_symbol->symbol_list->push_back(info);
                        }
                    }
                }
            }
        }

        cmdPtr += loadCmd->cmdsize;
    }

    std::sort(image_symbol->symbol_list->begin(), image_symbol->symbol_list->end(), STRUCT_NLIST_compare);

    return image_symbol;
}

bool ksdl_dladdr_use_cache(const uintptr_t address, Dl_info *const info) {
    if (!s_enableLocalSymbolicate) {
        return false;
    }

    static std::unordered_map<uint32_t, image_symbol_infos *> s_symbol_map;
    static std::atomic_flag s_lock = ATOMIC_FLAG_INIT;

    info->dli_fname = NULL;
    info->dli_fbase = NULL;
    info->dli_sname = NULL;
    info->dli_saddr = NULL;

    uint32_t idx = UINT_MAX;

    while (s_lock.test_and_set()) {
    };

    for (auto iter = s_symbol_map.begin(); iter != s_symbol_map.end(); ++iter) {
        uintptr_t addressWSlide = address - (uintptr_t)__ks_dyld_get_image_vmaddr_slide(iter->first);
        if (addressWSlide >= iter->second->vm_str && addressWSlide < iter->second->vm_end) {
            idx = iter->first;
            break;
        }
    }

    s_lock.clear();

    if (idx == UINT_MAX) {
        idx = imageIndexContainingAddress(address);
    }

    if (idx == UINT_MAX) {
        return false;
    }

    const char *name = __ks_dyld_get_image_name(idx);
    if (name == NULL) {
        return false;
    }

    const struct mach_header *header = __ks_dyld_get_image_header(idx);
    if (header == NULL) {
        return false;
    }

    const uintptr_t imageVMAddrSlide = (uintptr_t)__ks_dyld_get_image_vmaddr_slide(idx);
    const uintptr_t addressWithSlide = address - imageVMAddrSlide;
    const uintptr_t segmentBase = segmentBaseOfImageIndex(idx) + imageVMAddrSlide;

    if (segmentBase == 0) {
        return false;
    }

    image_symbol_infos *image_symbol = NULL;

    while (s_lock.test_and_set()) {
    };

    auto iter = s_symbol_map.find(idx);
    if (iter == s_symbol_map.end()) {
        image_symbol = ksdl_build_symbol_list(header, segmentBase);
        image_symbol->image_index = idx;
        s_symbol_map.insert(std::make_pair(idx, image_symbol));
    } else {
        image_symbol = iter->second;
    }

    s_lock.clear();

    if (image_symbol == NULL || image_symbol->symbol_list->size() == 0) {
        return false;
    }

    info->dli_fname = name;
    info->dli_fbase = (void *)header;

    int low = 0;
    int high = (int)image_symbol->symbol_list->size() - 1;
    symbol_info best = { 0 };
    while (low <= high) {
        int midd = ((low + high) >> 1);

        symbol_info curr = image_symbol->symbol_list->at(midd);
        if (addressWithSlide >= curr.detail.n_value) {
            best = curr;
            low = midd + 1;
        } else {
            high = midd - 1;
        }
    }

    if (best.value != 0) {
        info->dli_saddr = (void *)(best.detail.n_value + imageVMAddrSlide);
        info->dli_sname = (char *)((intptr_t)image_symbol->string_table + (intptr_t)best.detail.n_strx);
        if (*info->dli_sname == '_') {
            info->dli_sname++;
        }
        return true;
    } else {
        return false;
    }
}

int ksdl_imageCount() {
    return (int)__ks_dyld_image_count();
}

bool ksdl_getBinaryImage(int index, KSBinaryImage *buffer) {
    const struct mach_header *header = __ks_dyld_get_image_header((unsigned)index);
    if (header == NULL) {
        return false;
    }

    const char *name = __ks_dyld_get_image_name((unsigned)index);
    if (name == NULL) {
        return false;
    }

    uintptr_t cmdPtr = __ks_first_cmd_after_header(header);
    if (cmdPtr == 0) {
        return false;
    }

    // Look for the TEXT segment to get the image size.
    // Also look for a UUID command.
    uint64_t imageSize = 0;
    uint64_t imageVmAddr = 0;
    uint64_t version = 0;
    uint8_t *uuid = NULL;

    for (uint32_t iCmd = 0; iCmd < header->ncmds; iCmd++) {
        struct load_command *loadCmd = (struct load_command *)cmdPtr;
        switch (loadCmd->cmd) {
            case LC_SEGMENT: {
                struct segment_command *segCmd = (struct segment_command *)cmdPtr;
                if (strcmp(segCmd->segname, SEG_TEXT) == 0) {
                    imageSize = segCmd->vmsize;
                    imageVmAddr = segCmd->vmaddr;
                }
                break;
            }
            case LC_SEGMENT_64: {
                struct segment_command_64 *segCmd = (struct segment_command_64 *)cmdPtr;
                if (strcmp(segCmd->segname, SEG_TEXT) == 0) {
                    imageSize = segCmd->vmsize;
                    imageVmAddr = segCmd->vmaddr;
                }
                break;
            }
            case LC_UUID: {
                struct uuid_command *uuidCmd = (struct uuid_command *)cmdPtr;
                uuid = uuidCmd->uuid;
                break;
            }
            case LC_ID_DYLIB: {
                struct dylib_command *dc = (struct dylib_command *)cmdPtr;
                version = dc->dylib.current_version;
                break;
            }
        }
        cmdPtr += loadCmd->cmdsize;
    }

    buffer->address = (uintptr_t)header;
    buffer->vmAddress = imageVmAddr;
    buffer->size = imageSize;
    buffer->name = name;
    buffer->uuid = uuid;
    buffer->cpuType = header->cputype;
    buffer->cpuSubType = header->cpusubtype;
    buffer->majorVersion = version >> 16;
    buffer->minorVersion = (version >> 8) & 0xff;
    buffer->revisionVersion = version & 0xff;

    return true;
}
