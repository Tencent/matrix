//
// Created by 邓沛堆 on 2020-05-07.
//

#ifndef LIBWXPERF_JNI_EGLHOOK_H
#define LIBWXPERF_JNI_EGLHOOK_H

#include "HookCommon.h"
#include <EGL/egl.h>
#include "Utils.h"

DECLARE_HOOK_ORIG(EGLContext, eglCreateContext, EGLDisplay dpy, EGLConfig config,
                  EGLContext share_context, const EGLint *attrib_list);

DECLARE_HOOK_ORIG(EGLBoolean, eglDestroyContext, EGLDisplay dpy, EGLContext ctx);

DECLARE_HOOK_ORIG(EGLSurface, eglCreatePbufferSurface, EGLDisplay dpy, EGLContext ctx,
                  const EGLint *attrib_list, int offset);

DECLARE_HOOK_ORIG(EGLSurface, eglCreateWindowSurface, EGLDisplay dpy, EGLConfig config,
                  EGLNativeWindowType window, const EGLint *attrib_list);

DECLARE_HOOK_ORIG(EGLBoolean, eglDestorySurface, EGLDisplay dpy, EGLSurface surface)


#endif //LIBWXPERF_JNI_EGLHOOK_H