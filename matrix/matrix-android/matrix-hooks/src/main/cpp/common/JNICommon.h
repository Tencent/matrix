//
// Created by Yves on 2020-03-16.
//

#ifndef LIBMATRIX_HOOK_JNICOMMON_H
#define LIBMATRIX_HOOK_JNICOMMON_H

#include <jni.h>
#include <bits/pthread_types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern JavaVM *m_java_vm;

extern jclass m_class_HookManager;
extern jmethodID m_method_getStack;

#ifdef __cplusplus
}
#endif

#endif //LIBMATRIX_HOOK_JNICOMMON_H
