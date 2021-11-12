//
// Created by M.D. on 2021/10/29.
//

#ifndef MATRIX_ANDROID_SELF_DLFCN_H
#define MATRIX_ANDROID_SELF_DLFCN_H

extern "C" {

void self_dlfcn_mode(int api);

void self_dlclose(void *handle);

void *self_dlopen(const char *filename);

void *self_dlsym(void *handle, const char *name);

}

#endif //MATRIX_ANDROID_SELF_DLFCN_H
