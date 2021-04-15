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

extern jclass m_class_EglHook;
extern jmethodID m_method_egl_create_context;
extern jmethodID m_method_egl_destroy_context;
extern jmethodID m_method_egl_create_window_surface;
extern jmethodID m_method_egl_create_pbuffer_surface;
extern jmethodID m_method_egl_destroy_surface;

#ifdef __cplusplus
}
#endif

#endif //LIBMATRIX_HOOK_JNICOMMON_H
