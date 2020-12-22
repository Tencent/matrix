//
// Created by Yves on 2020/7/9.
//

#ifndef LIBWXPERF_JNI_PTHREADEXT_H
#define LIBWXPERF_JNI_PTHREADEXT_H

#include <pthread.h>

#define THREAD_NAME_LEN 16

int pthread_getname_ext(pthread_t __pthread, char *__buf, size_t __n);

void pthread_ext_init();

int pthread_getattr_ext(pthread_t pthread, pthread_attr_t* attr);

#endif //LIBWXPERF_JNI_PTHREADEXT_H
