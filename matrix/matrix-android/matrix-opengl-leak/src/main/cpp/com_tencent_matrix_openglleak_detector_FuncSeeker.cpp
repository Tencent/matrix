//
// Created by 邓沛堆 on 2020-04-26.
//

#include <jni.h>
#include <android/log.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <string>
#include <sys/system_properties.h>
#include "com_tencent_matrix_openglleak_detector_FuncSeeker.h"
#include "get_tls.h"
#include "type.h"

#define HOOK_O_FUNC(func, params...) func(params)

#define LOG_TAG "matrix.opengl"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static System_GlNormal_TYPE _system_glGenNormal = NULL;
static int i_glGenNormal = 0;
static bool has_hook_glGenNormal = false;

GL_APICALL void GL_APIENTRY _my_glNormal(GLsizei n, GLuint *normal) {
    if (!has_hook_glGenNormal) {
        has_hook_glGenNormal = true;
    }

    _system_glGenNormal(n, normal);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_tencent_matrix_openglleak_detector_FuncSeeker_getTargetFuncIndex(JNIEnv *env, jclass,
                                                                          jstring target_func_name) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return 0;
    }

    System_GlNormal_TYPE target_func = get_target_func_ptr(env->GetStringUTFChars(target_func_name, JNI_FALSE));
    if (NULL == target_func) {
        return 0;
    }

    for (i_glGenNormal = 0; i_glGenNormal < 500; i_glGenNormal++) {
        if (has_hook_glGenNormal) {
            i_glGenNormal = i_glGenNormal - 1;

            void **method = (void **) (&hooks->gl.foo1 + i_glGenNormal);
            *method = (void *) _system_glGenNormal;
            break;
        }

        if (_system_glGenNormal != NULL) {
            void **method = (void **) (&hooks->gl.foo1 + (i_glGenNormal - 1));
            *method = (void *) _system_glGenNormal;
        }

        void **replaceMethod = (void **) (&hooks->gl.foo1 + i_glGenNormal);
        _system_glGenNormal = (System_GlNormal_TYPE) *replaceMethod;

        *replaceMethod = (void *) _my_glNormal;

        // 验证是否已经拿到偏移值
        HOOK_O_FUNC(target_func, 0, NULL);
    }

    if (i_glGenNormal == 500) {
        i_glGenNormal = 0;
    }

    // release
    _system_glGenNormal = NULL;
    has_hook_glGenNormal = false;
    int result = i_glGenNormal;
    i_glGenNormal = 0;

    return result;
}

static System_GlGetError_TYPE _system_glGetError = NULL;
static int i_glGetError = 0;
static bool has_hook_glGetError = false;

GL_APICALL int GL_APIENTRY _my_glGetError() {
    if (!has_hook_glGetError) {
        has_hook_glGetError = true;
    }

    return _system_glGetError();
}

extern "C" JNIEXPORT jint
JNICALL Java_com_tencent_matrix_openglleak_detector_FuncSeeker_getGlGetErrorIndex
        (JNIEnv *, jclass) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return -1;
    }

    for (i_glGetError = 0; i_glGetError < 500; i_glGetError++) {
        if (has_hook_glGetError) {
            i_glGetError = i_glGetError - 1;

            void **method = (void **) (&hooks->gl.foo1 + i_glGetError);
            *method = (void *) _system_glGetError;
            break;
        }

        if (_system_glGetError != NULL) {
            void **method = (void **) (&hooks->gl.foo1 + (i_glGetError - 1));
            *method = (void *) _system_glGetError;
        }

        void **replaceMethod = (void **) (&hooks->gl.foo1 + i_glGetError);
        _system_glGetError = (System_GlGetError_TYPE) *replaceMethod;

        *replaceMethod = (void *) _my_glGetError;

        glGetError();
    }

    // release
    _system_glGetError = NULL;
    has_hook_glGetError = false;
    int result = i_glGetError;
    i_glGetError = 0;

    return result;
}

static System_GlBind_TYPE _system_glBind = NULL;
static int i_glBind = 0;
static bool has_hook_glBind = false;

GL_APICALL void GL_APIENTRY _my_glBind(GLenum target, GLuint resourceId) {
    if (!has_hook_glBind) {
        has_hook_glBind = true;
    }

    _system_glBind(target, resourceId);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_tencent_matrix_openglleak_detector_FuncSeeker_getBindFuncIndex(JNIEnv *env, jclass clazz,
                                                                        jstring bind_func_name) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return 0;
    }

    System_GlBind_TYPE bind_func = get_bind_func_ptr(env->GetStringUTFChars(bind_func_name, JNI_FALSE));
    if (NULL == bind_func_name) {
        return 0;
    }

    for (i_glBind = 0; i_glBind < 500; i_glBind++) {
        if (has_hook_glBind) {
            i_glBind = i_glBind - 1;

            void **method = (void **) (&hooks->gl.foo1 + i_glBind);
            *method = (void *) _system_glBind;
            break;
        }

        if (_system_glBind != NULL) {
            void **method = (void **) (&hooks->gl.foo1 + (i_glBind - 1));
            *method = (void *) _system_glBind;
        }

        void **replaceMethod = (void **) (&hooks->gl.foo1 + i_glBind);
        _system_glBind = (System_GlBind_TYPE) *replaceMethod;

        *replaceMethod = (void *) _my_glBind;

        // 验证是否已经拿到偏移值
        HOOK_O_FUNC(bind_func, 0, NULL);
    }

    if (i_glBind == 500) {
        i_glBind = 0;
    }

    // release
    _system_glBind = NULL;
    has_hook_glBind = false;
    int result = i_glBind;
    i_glBind = 0;

    return result;
}

static System_GlTexImage2D _system_glTexImage2D = NULL;
static int i_glTexImage2D = 0;
static bool has_hook_glTexImage2D = false;

GL_APICALL void GL_APIENTRY
_my_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                 GLint border, GLenum format, GLenum type, const void *pixels) {
    if (!has_hook_glTexImage2D) {
        has_hook_glTexImage2D = true;
    }

    _system_glTexImage2D(target, level, internalformat, width, height, border, format, type,
                         pixels);
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_tencent_matrix_openglleak_detector_FuncSeeker_getGlTexImage2DIndex(JNIEnv *env, jclass clazz) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return -1;
    }

    for (i_glTexImage2D = 0; i_glTexImage2D < 1000; i_glTexImage2D++) {
        if (has_hook_glTexImage2D) {
            i_glTexImage2D = i_glTexImage2D - 1;

            void **method = (void **) (&hooks->gl.foo1 + i_glTexImage2D);
            *method = (void *) _system_glTexImage2D;
            break;
        }

        if (_system_glTexImage2D != NULL) {
            void **method = (void **) (&hooks->gl.foo1 + (i_glTexImage2D - 1));
            *method = (void *) _system_glTexImage2D;
        }

        void **replaceMethod = (void **) (&hooks->gl.foo1 + i_glTexImage2D);
        _system_glTexImage2D = (System_GlTexImage2D) *replaceMethod;

        *replaceMethod = (void *) _my_glTexImage2D;

        glTexImage2D(0, 0, 0, 0, 0, 0, 0, 0, NULL);
    }

    // release
    _system_glTexImage2D = NULL;
    has_hook_glTexImage2D = false;
    int result = i_glTexImage2D;
    i_glTexImage2D = 0;

    return result;
}