//
// Created by Yves on 2020/7/9.
//

#ifndef LIBWXPERF_JNI_REENTRANTPREVENTION_H
#define LIBWXPERF_JNI_REENTRANTPREVENTION_H

void rp_init();

bool rp_acquire() ;

void rp_release();

#endif //LIBWXPERF_JNI_REENTRANTPREVENTION_H
