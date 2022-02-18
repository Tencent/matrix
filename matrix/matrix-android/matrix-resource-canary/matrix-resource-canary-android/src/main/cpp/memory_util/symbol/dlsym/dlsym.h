//
// Created by M.D. on 2021/10/29.
//

#ifndef MATRIX_ANDROID_DLSYM_H
#define MATRIX_ANDROID_DLSYM_H

extern "C" {

void ds_mode(int api);

void ds_close(void *handle);

void ds_clean(void *handle);

void *ds_open(const char *filename);

void *ds_find(void *handle, const char *name);

}

#endif //MATRIX_ANDROID_DLSYM_H
