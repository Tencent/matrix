//
// Created by YinSheng Tang on 2021/7/21.
//

#ifndef MATRIX_ANDROID_SEMI_DLFCN_H
#define MATRIX_ANDROID_SEMI_DLFCN_H


#include <stddef.h>
#include <dlfcn.h>
#include <link.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*iterate_callback)(struct dl_phdr_info *info, size_t info_size, void *data);

int semi_dl_iterate_phdr(iterate_callback cb, void *data);

void* semi_dlopen(const char* pathname);

void* semi_dlsym(const void* semi_hlib, const char* sym_name);

void semi_dlclose(void* semi_hlib);

#ifdef __cplusplus
}
#endif


#endif //MATRIX_ANDROID_SEMI_DLFCN_H
