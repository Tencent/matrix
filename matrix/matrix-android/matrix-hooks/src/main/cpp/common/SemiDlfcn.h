//
// Created by YinSheng Tang on 2021/7/9.
//

#ifndef MATRIX_ANDROID_SEMIDLFCN_H
#define MATRIX_ANDROID_SEMIDLFCN_H


#include "Macros.h"

namespace matrix {
    EXPORT void* SemiDlOpen(const char* name);
    EXPORT void* SemiDlSym(const void* semi_hlib, const char* sym_name);
    EXPORT void SemiDlClose(void* semi_hlib);
}


#endif //MATRIX_ANDROID_SEMIDLFCN_H
