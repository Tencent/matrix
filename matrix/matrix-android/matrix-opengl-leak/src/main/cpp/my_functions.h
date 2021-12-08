//
// Created by 邓沛堆 on 2020-05-28.
//

#include "type.h"
#include <GLES2/gl2.h>
#include <jni.h>
#include <string>
#include <sstream>
#include <thread>
#include "BacktraceDefine.h"
#include "Backtrace.h"
#include <linux/prctl.h>
#include <sys/prctl.h>

#ifndef OPENGL_API_HOOK_MY_FUNCTIONS_H
#define OPENGL_API_HOOK_MY_FUNCTIONS_H

using namespace std;

#define MEMHOOK_BACKTRACE_MAX_FRAMES MAX_FRAME_SHORT
#define RENDER_THREAD_NAME "RenderThread"
#define max(a, b) ((a) > (b) ? (a) : (b))

static System_GlNormal_TYPE system_glGenTextures = NULL;
static System_GlNormal_TYPE system_glDeleteTextures = NULL;
static System_GlNormal_TYPE system_glGenBuffers = NULL;
static System_GlNormal_TYPE system_glDeleteBuffers = NULL;
static System_GlNormal_TYPE system_glGenFramebuffers = NULL;
static System_GlNormal_TYPE system_glDeleteFramebuffers = NULL;
static System_GlNormal_TYPE system_glGenRenderbuffers = NULL;
static System_GlNormal_TYPE system_glDeleteRenderbuffers = NULL;
static System_GlGetError_TYPE system_glGetError = NULL;
static System_GlTexImage2D system_glTexImage2D = NULL;
static System_GlTexImage3D system_glTexImage3D = NULL;
static System_GlBind_TYPE system_glBindTexture = NULL;
static System_GlBind_TYPE system_glBindBuffer = NULL;
static System_GlBind_TYPE system_glBindFramebuffer = NULL;
static System_GlBind_TYPE system_glBindRenderbuffer = NULL;
static System_GlBufferData system_glBufferData = NULL;
static System_GlRenderbufferStorage system_glRenderbufferStorage = NULL;

static JavaVM *m_java_vm;

static jclass class_OpenGLHook;
static jmethodID method_onGlGenTextures;
static jmethodID method_onGlDeleteTextures;
static jmethodID method_onGlGenBuffers;
static jmethodID method_onGlDeleteBuffers;
static jmethodID method_onGlGenFramebuffers;
static jmethodID method_onGlDeleteFramebuffers;
static jmethodID method_onGlGenRenderbuffers;
static jmethodID method_onGlDeleteRenderbuffers;
static jmethodID method_getStack;
static jmethodID method_onGetError;
static jmethodID method_onGlBindTexture;
static jmethodID method_onGlBindBuffer;
static jmethodID method_onGlBindFramebuffer;
static jmethodID method_onGlBindRenderbuffer;
static jmethodID method_onGlTexImage2D;
static jmethodID method_onGlTexImage3D;
static jmethodID method_onGlBufferData;
static jmethodID method_onGlRenderbufferStorage;

const size_t BUF_SIZE = 1024;

static pthread_once_t g_onceInitTls = PTHREAD_ONCE_INIT;
static pthread_key_t g_tlsJavaEnv;

map<thread::id, bool> jni_env_status;

static bool is_stacktrace_enabled = true;

void enable_stacktrace(bool enable) {
    is_stacktrace_enabled = enable;
}

static bool is_javastack_enabled = true;

void enable_javastack(bool enable) {
    is_javastack_enabled = enable;
}

void thread_id_to_string(thread::id thread_id, char *&result) {
    stringstream stream;
    stream << thread_id;
    result = new char[stream.str().size() + 1];
    strcpy(result, stream.str().c_str());
}

inline void get_thread_name(char *thread_name) {
    prctl(PR_GET_NAME, (char *) (thread_name));
}

inline bool is_render_thread() {
    bool result = false;
    char *thread_name = static_cast<char *>(malloc(BUF_SIZE));
    get_thread_name(thread_name);
    if (strcmp(RENDER_THREAD_NAME, thread_name) == 0) {
        result = true;
    }
    if (thread_name != nullptr) {
        free(thread_name);
    }
    return result;
}

JNIEnv *GET_ENV() {
    JNIEnv *env;
    int ret = m_java_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    thread::id thread_id = this_thread::get_id();
    map<thread::id, bool>::iterator it = jni_env_status.find(thread_id);
    if (it == jni_env_status.end()) {
        jni_env_status[thread_id] = ret;
    }
    if (ret != JNI_OK) {
        pthread_once(&g_onceInitTls, []() {
            pthread_key_create(&g_tlsJavaEnv, [](void *d) {
                if (d && m_java_vm)
                    m_java_vm->DetachCurrentThread();
                thread::id thread_id = this_thread::get_id();
                jni_env_status.erase(thread_id);
            });
        });

        if (m_java_vm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
            pthread_setspecific(g_tlsJavaEnv, reinterpret_cast<const void *>(1));
        } else {
            env = nullptr;
        }
    }
    return env;
}

bool get_java_stacktrace(char *__stack, size_t __size) {

    if (!__stack) {
        return false;
    }

    JNIEnv *env = GET_ENV();

    jstring j_stacktrace = (jstring) env->CallStaticObjectMethod(class_OpenGLHook, method_getStack);

    const char *stack = env->GetStringUTFChars(j_stacktrace, NULL);
    if (stack) {
        const size_t cpy_len = std::min(strlen(stack) + 1, __size - 1);
        memcpy(__stack, stack, cpy_len);
        __stack[cpy_len] = '\0';
    } else {
        strncpy(__stack, "\tget java stacktrace failed", __size);
    }
    env->ReleaseStringUTFChars(j_stacktrace, stack);
    env->DeleteLocalRef(j_stacktrace);

    return true;

}

char *get_java_stack() {
    char *buf = static_cast<char *>(malloc(BUF_SIZE));
    if (buf) {
        get_java_stacktrace(buf, BUF_SIZE);
    }
    return buf;
}

void get_thread_id_string(char *&result) {
    thread::id thread_id = this_thread::get_id();
    thread_id_to_string(thread_id, result);
}

bool is_need_get_java_stack() {
    thread::id thread_id = this_thread::get_id();
    return jni_env_status[thread_id] == JNI_OK;
}

GL_APICALL void GL_APIENTRY my_glGenTextures(GLsizei n, GLuint *textures) {
    if (NULL != system_glGenTextures) {
        system_glGenTextures(n, textures);

        if (is_render_thread()) {
            return;
        }

        JNIEnv *env = GET_ENV();

        int *result = new int[n];
        for (int i = 0; i < n; i++) {
            result[i] = *(textures + i);
        }
        jintArray newArr = env->NewIntArray(n);
        env->SetIntArrayRegion(newArr, 0, n, result);

        char *thread_id = nullptr;
        get_thread_id_string(thread_id);
        jstring j_thread_id = env->NewStringUTF(thread_id);

        wechat_backtrace::Backtrace *backtracePrt = 0;
        if (is_stacktrace_enabled) {
            wechat_backtrace::Backtrace backtrace_zero = BACKTRACE_INITIALIZER(
                    MEMHOOK_BACKTRACE_MAX_FRAMES);

            backtracePrt = new wechat_backtrace::Backtrace;
            backtracePrt->max_frames = backtrace_zero.max_frames;
            backtracePrt->frame_size = backtrace_zero.frame_size;
            backtracePrt->frames = backtrace_zero.frames;

            wechat_backtrace::unwind_adapter(backtracePrt->frames.get(), backtracePrt->max_frames,
                                             backtracePrt->frame_size);
        }

        jstring java_stack;
        char *javaStack = nullptr;
        if (is_javastack_enabled && is_need_get_java_stack()) {
            javaStack = get_java_stack();
            java_stack = env->NewStringUTF(javaStack);
        } else {
            java_stack = env->NewStringUTF("");
        }

        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlGenTextures, newArr, j_thread_id,
                                  java_stack, (int64_t) backtracePrt);

        delete[] result;
        if (is_javastack_enabled && javaStack != nullptr) {
            free(javaStack);
        }

        env->DeleteLocalRef(newArr);
        env->DeleteLocalRef(j_thread_id);
        env->DeleteLocalRef(java_stack);

        if (thread_id != nullptr) {
            delete[] thread_id;
            thread_id = nullptr;
        }
    }
}

GL_APICALL void GL_APIENTRY my_glDeleteTextures(GLsizei n, GLuint *textures) {
    if (NULL != system_glDeleteTextures) {
        system_glDeleteTextures(n, textures);

        if (is_render_thread()) {
            return;
        }

        JNIEnv *env = GET_ENV();

        int *result = new int[n];
        for (int i = 0; i < n; i++) {
            result[i] = *(textures + i);
        }
        jintArray newArr = env->NewIntArray(n);
        env->SetIntArrayRegion(newArr, 0, n, result);

        char *thread_id = nullptr;
        get_thread_id_string(thread_id);
        jstring j_thread_id = env->NewStringUTF(thread_id);

        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlDeleteTextures, newArr, j_thread_id);
        delete[] result;
        env->DeleteLocalRef(newArr);
        env->DeleteLocalRef(j_thread_id);
        if (thread_id != nullptr) {
            delete[] thread_id;
            thread_id = nullptr;
        }
    }
}

GL_APICALL void GL_APIENTRY my_glGenBuffers(GLsizei n, GLuint *buffers) {
    if (NULL != system_glGenBuffers) {
        system_glGenBuffers(n, buffers);

        if (is_render_thread()) {
            return;
        }

        JNIEnv *env = GET_ENV();

        int *result = new int[n];
        for (int i = 0; i < n; i++) {
            result[i] = *(buffers + i);
        }
        jintArray newArr = env->NewIntArray(n);
        env->SetIntArrayRegion(newArr, 0, n, result);

        char *thread_id = nullptr;
        get_thread_id_string(thread_id);
        jstring j_thread_id = env->NewStringUTF(thread_id);

        wechat_backtrace::Backtrace *backtracePrt = 0;
        if (is_stacktrace_enabled) {
            wechat_backtrace::Backtrace backtrace_zero = BACKTRACE_INITIALIZER(
                    MEMHOOK_BACKTRACE_MAX_FRAMES);

            backtracePrt = new wechat_backtrace::Backtrace;
            backtracePrt->max_frames = backtrace_zero.max_frames;
            backtracePrt->frame_size = backtrace_zero.frame_size;
            backtracePrt->frames = backtrace_zero.frames;

            wechat_backtrace::unwind_adapter(backtracePrt->frames.get(), backtracePrt->max_frames,
                                             backtracePrt->frame_size);
        }

        jstring java_stack;
        char *javaStack = nullptr;
        if (is_javastack_enabled && is_need_get_java_stack()) {
            javaStack = get_java_stack();
            java_stack = env->NewStringUTF(javaStack);
        } else {
            java_stack = env->NewStringUTF("");
        }

        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlGenBuffers, newArr, j_thread_id,
                                  java_stack, (int64_t) backtracePrt);

        delete[] result;
        if (is_javastack_enabled && javaStack != nullptr) {
            free(javaStack);
        }

        env->DeleteLocalRef(newArr);
        env->DeleteLocalRef(j_thread_id);
        env->DeleteLocalRef(java_stack);
        if (thread_id != nullptr) {
            delete[] thread_id;
            thread_id = nullptr;
        }
    }
}

GL_APICALL void GL_APIENTRY my_glDeleteBuffers(GLsizei n, GLuint *buffers) {
    if (NULL != system_glDeleteBuffers) {
        system_glDeleteBuffers(n, buffers);

        if (is_render_thread()) {
            return;
        }

        JNIEnv *env = GET_ENV();

        int *result = new int[n];
        for (int i = 0; i < n; i++) {
            result[i] = *(buffers + i);
        }
        jintArray newArr = env->NewIntArray(n);
        env->SetIntArrayRegion(newArr, 0, n, result);

        char *thread_id = nullptr;
        get_thread_id_string(thread_id);
        jstring j_thread_id = env->NewStringUTF(thread_id);

        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlDeleteBuffers, newArr, j_thread_id);
        delete[] result;
        env->DeleteLocalRef(newArr);
        env->DeleteLocalRef(j_thread_id);
        if (thread_id != nullptr) {
            delete[] thread_id;
            thread_id = nullptr;
        }
    }
}

GL_APICALL void GL_APIENTRY my_glGenFramebuffers(GLsizei n, GLuint *buffers) {
    if (NULL != system_glGenFramebuffers) {
        system_glGenFramebuffers(n, buffers);

        if (is_render_thread()) {
            return;
        }

        JNIEnv *env = GET_ENV();

        int *result = new int[n];
        for (int i = 0; i < n; i++) {
            result[i] = *(buffers + i);
        }
        jintArray newArr = env->NewIntArray(n);
        env->SetIntArrayRegion(newArr, 0, n, result);

        char *thread_id = nullptr;
        get_thread_id_string(thread_id);
        jstring j_thread_id = env->NewStringUTF(thread_id);

        wechat_backtrace::Backtrace *backtracePrt = 0;
        if (is_stacktrace_enabled) {
            wechat_backtrace::Backtrace backtrace_zero = BACKTRACE_INITIALIZER(
                    MEMHOOK_BACKTRACE_MAX_FRAMES);

            backtracePrt = new wechat_backtrace::Backtrace;
            backtracePrt->max_frames = backtrace_zero.max_frames;
            backtracePrt->frame_size = backtrace_zero.frame_size;
            backtracePrt->frames = backtrace_zero.frames;

            wechat_backtrace::unwind_adapter(backtracePrt->frames.get(), backtracePrt->max_frames,
                                             backtracePrt->frame_size);
        }

        jstring java_stack;
        char *javaStack = nullptr;
        if (is_javastack_enabled && is_need_get_java_stack()) {
            javaStack = get_java_stack();
            java_stack = env->NewStringUTF(javaStack);
        } else {
            java_stack = env->NewStringUTF("");
        }

        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlGenFramebuffers, newArr,
                                  j_thread_id, java_stack, (int64_t) backtracePrt);

        delete[] result;
        if (is_javastack_enabled && javaStack != nullptr) {
            free(javaStack);
        }

        env->DeleteLocalRef(newArr);
        env->DeleteLocalRef(j_thread_id);
        env->DeleteLocalRef(java_stack);
        if (thread_id != nullptr) {
            delete[] thread_id;
            thread_id = nullptr;
        }
    }
}

GL_APICALL void GL_APIENTRY my_glDeleteFramebuffers(GLsizei n, GLuint *buffers) {
    if (NULL != system_glDeleteFramebuffers) {
        system_glDeleteFramebuffers(n, buffers);

        if (is_render_thread()) {
            return;
        }

        JNIEnv *env = GET_ENV();

        int *result = new int[n];
        for (int i = 0; i < n; i++) {
            result[i] = *(buffers + i);
        }
        jintArray newArr = env->NewIntArray(n);
        env->SetIntArrayRegion(newArr, 0, n, result);

        char *thread_id = nullptr;
        get_thread_id_string(thread_id);
        jstring j_thread_id = env->NewStringUTF(thread_id);

        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlDeleteFramebuffers, newArr,
                                  j_thread_id);
        delete[] result;
        env->DeleteLocalRef(newArr);
        env->DeleteLocalRef(j_thread_id);
        if (thread_id != nullptr) {
            delete[] thread_id;
            thread_id = nullptr;
        }
    }
}

GL_APICALL void GL_APIENTRY my_glGenRenderbuffers(GLsizei n, GLuint *buffers) {
    if (NULL != system_glGenRenderbuffers) {
        system_glGenRenderbuffers(n, buffers);

        if (is_render_thread()) {
            return;
        }

        JNIEnv *env = GET_ENV();

        int *result = new int[n];
        for (int i = 0; i < n; i++) {
            result[i] = *(buffers + i);
        }
        jintArray newArr = env->NewIntArray(n);
        env->SetIntArrayRegion(newArr, 0, n, result);

        char *thread_id = nullptr;
        get_thread_id_string(thread_id);
        jstring j_thread_id = env->NewStringUTF(thread_id);

        wechat_backtrace::Backtrace *backtracePrt = 0;
        if (is_stacktrace_enabled) {
            wechat_backtrace::Backtrace backtrace_zero = BACKTRACE_INITIALIZER(
                    MEMHOOK_BACKTRACE_MAX_FRAMES);

            backtracePrt = new wechat_backtrace::Backtrace;
            backtracePrt->max_frames = backtrace_zero.max_frames;
            backtracePrt->frame_size = backtrace_zero.frame_size;
            backtracePrt->frames = backtrace_zero.frames;

            wechat_backtrace::unwind_adapter(backtracePrt->frames.get(), backtracePrt->max_frames,
                                             backtracePrt->frame_size);
        }

        jstring java_stack;
        char *javaStack = nullptr;
        if (is_javastack_enabled && is_need_get_java_stack()) {
            javaStack = get_java_stack();
            java_stack = env->NewStringUTF(javaStack);
        } else {
            java_stack = env->NewStringUTF("");
        }

        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlGenRenderbuffers, newArr,
                                  j_thread_id, java_stack, (int64_t) backtracePrt);

        delete[] result;
        if (is_javastack_enabled && javaStack != nullptr) {
            free(javaStack);
        }

        env->DeleteLocalRef(newArr);
        env->DeleteLocalRef(j_thread_id);
        env->DeleteLocalRef(java_stack);
        if (thread_id != nullptr) {
            delete[] thread_id;
            thread_id = nullptr;
        }
    }
}

GL_APICALL void GL_APIENTRY my_glDeleteRenderbuffers(GLsizei n, GLuint *buffers) {
    if (NULL != system_glDeleteRenderbuffers) {
        system_glDeleteRenderbuffers(n, buffers);

        if (is_render_thread()) {
            return;
        }

        JNIEnv *env = GET_ENV();

        int *result = new int[n];
        for (int i = 0; i < n; i++) {
            result[i] = *(buffers + i);
        }
        jintArray newArr = env->NewIntArray(n);
        env->SetIntArrayRegion(newArr, 0, n, result);

        char *thread_id = nullptr;
        get_thread_id_string(thread_id);
        jstring j_thread_id = env->NewStringUTF(thread_id);

        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlDeleteRenderbuffers, newArr,
                                  j_thread_id);
        delete[] result;
        env->DeleteLocalRef(newArr);
        env->DeleteLocalRef(j_thread_id);
        if (thread_id != nullptr) {
            delete[] thread_id;
            thread_id = nullptr;
        }
    }
}

GL_APICALL int GL_APIENTRY my_glGetError() {
    if (NULL != system_glGetError) {
        int result = system_glGetError();

        JNIEnv *env = GET_ENV();
        jint jresult = result;

        env->CallStaticVoidMethod(class_OpenGLHook, method_onGetError, jresult);

        return result;
    }

    return 0;
}

GL_APICALL void GL_APIENTRY
my_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                GLint border, GLenum format, GLenum type, const void *pixels) {
    if (NULL != system_glTexImage2D) {
        system_glTexImage2D(target, level, internalformat, width, height, border, format, type,
                            pixels);

        if (is_render_thread()) {
            return;
        }
        int pixel = Utils::getSizeOfPerPixel(internalformat, format, type);
        long size = width * height * pixel;
        JNIEnv *env = GET_ENV();

        wechat_backtrace::Backtrace *backtracePrt = 0;
        if (is_stacktrace_enabled) {
            wechat_backtrace::Backtrace backtrace_zero = BACKTRACE_INITIALIZER(
                    MEMHOOK_BACKTRACE_MAX_FRAMES);

            backtracePrt = new wechat_backtrace::Backtrace;
            backtracePrt->max_frames = backtrace_zero.max_frames;
            backtracePrt->frame_size = backtrace_zero.frame_size;
            backtracePrt->frames = backtrace_zero.frames;

            wechat_backtrace::unwind_adapter(backtracePrt->frames.get(), backtracePrt->max_frames,
                                             backtracePrt->frame_size);
        }

        jstring java_stack;
        jstring local_java_stack;
        char *javaStack = nullptr;
        if (is_javastack_enabled && is_need_get_java_stack()) {
            javaStack = get_java_stack();
            local_java_stack = env->NewStringUTF(javaStack);
        } else {
            local_java_stack = env->NewStringUTF("");
        }

        java_stack = static_cast<jstring>(env->NewGlobalRef(local_java_stack));
        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlTexImage2D, target, level,
                                  internalformat, width, height, border, format, type, size,
                                  java_stack, (int64_t) backtracePrt);

        if (is_javastack_enabled && javaStack != nullptr) {
            free(javaStack);
        }
        env->DeleteLocalRef(local_java_stack);
        env->DeleteGlobalRef(java_stack);
    }
}

GL_APICALL void GL_APIENTRY
my_glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels) {
    if (NULL != system_glTexImage3D) {
        system_glTexImage3D(target, level, internalformat, width, height, depth, border, format,
                            type, pixels);

        if (is_render_thread()) {
            return;
        }
        int pixel = Utils::getSizeOfPerPixel(internalformat, format, type);
        long size = width * height * depth * pixel;
        JNIEnv *env = GET_ENV();

        wechat_backtrace::Backtrace *backtracePrt = 0;
        if (is_stacktrace_enabled) {
            wechat_backtrace::Backtrace backtrace_zero = BACKTRACE_INITIALIZER(
                    MEMHOOK_BACKTRACE_MAX_FRAMES);

            backtracePrt = new wechat_backtrace::Backtrace;
            backtracePrt->max_frames = backtrace_zero.max_frames;
            backtracePrt->frame_size = backtrace_zero.frame_size;
            backtracePrt->frames = backtrace_zero.frames;

            wechat_backtrace::unwind_adapter(backtracePrt->frames.get(), backtracePrt->max_frames,
                                             backtracePrt->frame_size);
        }

        jstring java_stack;
        jstring local_java_stack;
        char *javaStack = nullptr;
        if (is_javastack_enabled && is_need_get_java_stack()) {
            javaStack = get_java_stack();
            local_java_stack = env->NewStringUTF(javaStack);
        } else {
            local_java_stack = env->NewStringUTF("");
        }

        java_stack = static_cast<jstring>(env->NewGlobalRef(local_java_stack));
        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlTexImage3D, target, level,
                                  internalformat, width, height, depth, border, format, type, size,
                                  java_stack, (int64_t) backtracePrt);

        if (is_javastack_enabled && javaStack != nullptr) {
            free(javaStack);
        }
        env->DeleteLocalRef(local_java_stack);
        env->DeleteGlobalRef(java_stack);
    }
}


GL_APICALL void GL_APIENTRY my_glBindTexture(GLenum target, GLuint resourceId) {
    if (NULL != system_glBindTexture) {
        system_glBindTexture(target, resourceId);

        if (is_render_thread()) {
            return;
        }
        JNIEnv *env = GET_ENV();
        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlBindTexture, target, resourceId);
    }
}

GL_APICALL void GL_APIENTRY my_glBindBuffer(GLenum target, GLuint resourceId) {
    if (NULL != system_glBindTexture) {
        system_glBindBuffer(target, resourceId);

        if (is_render_thread()) {
            return;
        }
        JNIEnv *env = GET_ENV();
        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlBindBuffer, target, resourceId);

    }
}

GL_APICALL void GL_APIENTRY my_glBindFramebuffer(GLenum target, GLuint resourceId) {
    if (NULL != system_glBindTexture) {
        system_glBindFramebuffer(target, resourceId);

        if (is_render_thread()) {
            return;
        }
        JNIEnv *env = GET_ENV();
        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlBindFramebuffer, target, resourceId);

    }
}

GL_APICALL void GL_APIENTRY my_glBindRenderbuffer(GLenum target, GLuint resourceId) {
    if (NULL != system_glBindTexture) {
        system_glBindRenderbuffer(target, resourceId);

        if (is_render_thread()) {
            return;
        }
        JNIEnv *env = GET_ENV();
        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlBindRenderbuffer, target,
                                  resourceId);
    }
}

GL_APICALL void GL_APIENTRY
my_glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage) {
    if (NULL != system_glBindTexture) {
        system_glBufferData(target, size, data, usage);

        if (is_render_thread()) {
            return;
        }
        JNIEnv *env = GET_ENV();

        wechat_backtrace::Backtrace *backtracePrt = 0;
        if (is_stacktrace_enabled) {
            wechat_backtrace::Backtrace backtrace_zero = BACKTRACE_INITIALIZER(
                    MEMHOOK_BACKTRACE_MAX_FRAMES);

            backtracePrt = new wechat_backtrace::Backtrace;
            backtracePrt->max_frames = backtrace_zero.max_frames;
            backtracePrt->frame_size = backtrace_zero.frame_size;
            backtracePrt->frames = backtrace_zero.frames;

            wechat_backtrace::unwind_adapter(backtracePrt->frames.get(), backtracePrt->max_frames,
                                             backtracePrt->frame_size);
        }

        jstring java_stack;
        jstring local_java_stack;
        char *javaStack = nullptr;
        if (is_javastack_enabled && is_need_get_java_stack()) {
            javaStack = get_java_stack();
            local_java_stack = env->NewStringUTF(javaStack);
        } else {
            local_java_stack = env->NewStringUTF("");
        }

        java_stack = static_cast<jstring>(env->NewGlobalRef(local_java_stack));
        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlBufferData, target, size, usage,
                                  java_stack, (int64_t) backtracePrt);

        if (is_javastack_enabled && javaStack != nullptr) {
            free(javaStack);
        }
        env->DeleteLocalRef(local_java_stack);
        env->DeleteGlobalRef(java_stack);
    }
}

GL_APICALL void GL_APIENTRY
my_glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    if (NULL != system_glBindTexture) {
        system_glRenderbufferStorage(target, internalformat, width, height);

        if (is_render_thread()) {
            return;
        }
        JNIEnv *env = GET_ENV();

        wechat_backtrace::Backtrace *backtracePrt = 0;
        if (is_stacktrace_enabled) {
            wechat_backtrace::Backtrace backtrace_zero = BACKTRACE_INITIALIZER(
                    MEMHOOK_BACKTRACE_MAX_FRAMES);

            backtracePrt = new wechat_backtrace::Backtrace;
            backtracePrt->max_frames = backtrace_zero.max_frames;
            backtracePrt->frame_size = backtrace_zero.frame_size;
            backtracePrt->frames = backtrace_zero.frames;

            wechat_backtrace::unwind_adapter(backtracePrt->frames.get(), backtracePrt->max_frames,
                                             backtracePrt->frame_size);
        }

        jstring java_stack;
        jstring local_java_stack;
        char *javaStack = nullptr;
        if (is_javastack_enabled && is_need_get_java_stack()) {
            javaStack = get_java_stack();
            local_java_stack = env->NewStringUTF(javaStack);
        } else {
            local_java_stack = env->NewStringUTF("");
        }

        long size = Utils::getRenderbufferSizeByFormula(internalformat, width, height);
        java_stack = static_cast<jstring>(env->NewGlobalRef(local_java_stack));
        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlRenderbufferStorage, target,
                                  width, height, internalformat, size, java_stack,
                                  (int64_t) backtracePrt);

        if (is_javastack_enabled && javaStack != nullptr) {
            free(javaStack);
        }
        env->DeleteLocalRef(local_java_stack);
        env->DeleteGlobalRef(java_stack);
    }
}


#endif //OPENGL_API_HOOK_MY_FUNCTIONS_H