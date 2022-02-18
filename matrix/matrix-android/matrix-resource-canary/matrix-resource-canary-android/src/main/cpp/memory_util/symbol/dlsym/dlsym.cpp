//
// Created by M.D. on 2021/10/29.
//

#include "dlsym.h"

#include <dlfcn.h>
#include <EnhanceDlsym.h>

extern "C" {

static bool use_origin_ = false;

void ds_mode(int api) {
    use_origin_ = api < __ANDROID_API_N__;
}

void ds_close(void *handle) {
    if (use_origin_) dlclose(handle);
    else enhance::dlclose(handle);
}

void ds_clean(void* handle) {
    if (!use_origin_) enhance::dlclose(handle);
}

void *ds_open(const char *filename) {
    if (use_origin_) return dlopen(filename, RTLD_NOW);
    else return enhance::dlopen(filename, RTLD_NOW);
}

void *ds_find(void *handle, const char *name) {
    if (use_origin_) return dlsym(handle, name);
    else return enhance::dlsym(handle, name);
}

}