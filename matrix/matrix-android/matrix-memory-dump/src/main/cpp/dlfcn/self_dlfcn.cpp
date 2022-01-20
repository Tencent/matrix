//
// Created by M.D. on 2021/10/29.
//

#include "self_dlfcn.h"
#include <EnhanceDlsym.h>
#include <dlfcn.h>

extern "C" {

static bool use_origin_ = false;

void self_dlfcn_mode(int api) {
    use_origin_ = api < __ANDROID_API_N__;
}

void self_dlclose(void *handle) {
    if (use_origin_) dlclose(handle);
    else enhance::dlclose(handle);
}

void self_clean(void* handle) {
    if (!use_origin_) enhance::dlclose(handle);
}

void *self_dlopen(const char *filename) {
    if (use_origin_) return dlopen(filename, RTLD_NOW);
    else return enhance::dlopen(filename, RTLD_NOW);
}

void *self_dlsym(void *handle, const char *name) {
    if (use_origin_) return dlsym(handle, name);
    else return enhance::dlsym(handle, name);
}

}