//
// Created by Yves on 2020/7/9.
//

#ifndef LIBMATRIX_HOOK_REENTRANTPREVENTION_H
#define LIBMATRIX_HOOK_REENTRANTPREVENTION_H

void rp_init();

bool rp_acquire() ;

void rp_release();

#endif //LIBMATRIX_HOOK_REENTRANTPREVENTION_H
