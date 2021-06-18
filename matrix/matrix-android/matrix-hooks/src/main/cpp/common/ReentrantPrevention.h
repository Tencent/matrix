//
// Created by Yves on 2020/7/9.
//

#ifndef LIBMATRIX_HOOK_REENTRANTPREVENTION_H
#define LIBMATRIX_HOOK_REENTRANTPREVENTION_H


#include "Macros.h"

EXPORT void rp_init();

EXPORT bool rp_acquire();

EXPORT void rp_release();


#endif //LIBMATRIX_HOOK_REENTRANTPREVENTION_H
