//
// Created by 邓沛堆 on 2020-05-28.
//

#include "type.h"
#include <GLES2/gl2.h>
#include <jni.h>
#include <string>
#include <sstream>
#include <thread>
#include "unwindstack/Unwinder.h"
#include "BacktraceDefine.h"
#include "Backtrace.h"

#ifndef OPENGL_API_HOOK_MY_FUNCTIONS_H
#define OPENGL_API_HOOK_MY_FUNCTIONS_H

using namespace std;

#define MEMHOOK_BACKTRACE_MAX_FRAMES MAX_FRAME_SHORT

static System_GlNormal_TYPE system_glGenTextures = NULL;
static System_GlNormal_TYPE system_glDeleteTextures = NULL;
static System_GlNormal_TYPE system_glGenBuffers = NULL;
static System_GlNormal_TYPE system_glDeleteBuffers = NULL;
static System_GlNormal_TYPE system_glGenFramebuffers = NULL;
static System_GlNormal_TYPE system_glDeleteFramebuffers = NULL;
static System_GlNormal_TYPE system_glGenRenderbuffers = NULL;
static System_GlNormal_TYPE system_glDeleteRenderbuffers = NULL;
static System_GlGetError_TYPE system_glGetError = NULL;

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

const size_t BUF_SIZE = 1024;

static bool is_stacktrace_enabled = true;

void enable_stacktrace(bool enable) {
    is_stacktrace_enabled = enable;
}

void thread_id_to_string(thread::id thread_id, char *&result) {
    stringstream stream;
    stream << thread_id;
    result = new char[stream.str().size() + 1];
    stpcpy(result, stream.str().c_str());
}

JNIEnv *GET_ENV() {
    JNIEnv *env = NULL;
    if (m_java_vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        if (m_java_vm->AttachCurrentThread(&env, NULL) == JNI_OK) {
        } else {
            return nullptr;
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

GL_APICALL void GL_APIENTRY my_glGenTextures(GLsizei n, GLuint *textures) {
    if (NULL != system_glGenTextures) {
        system_glGenTextures(n, textures);

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

        wechat_backtrace::Backtrace* backtracePrt;
        if (is_stacktrace_enabled) {
            wechat_backtrace::Backtrace backtrace_zero = BACKTRACE_INITIALIZER(MEMHOOK_BACKTRACE_MAX_FRAMES);

            backtracePrt = new wechat_backtrace::Backtrace;
            backtracePrt->max_frames = backtrace_zero.max_frames;
            backtracePrt->frame_size = backtrace_zero.frame_size;
            backtracePrt->frames = backtrace_zero.frames;

            wechat_backtrace::unwind_adapter(backtracePrt->frames.get(), backtracePrt->max_frames, backtracePrt->frame_size);
        }

        char *javaStack = get_java_stack();
        jstring java_stack = env->NewStringUTF(javaStack);

        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlGenTextures, newArr, j_thread_id,
                                  java_stack, (int64_t) backtracePrt);

        delete[] result;
        free(javaStack);

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

        wechat_backtrace::Backtrace* backtracePrt;
        if (is_stacktrace_enabled) {
            wechat_backtrace::Backtrace backtrace_zero = BACKTRACE_INITIALIZER(MEMHOOK_BACKTRACE_MAX_FRAMES);

            backtracePrt = new wechat_backtrace::Backtrace;
            backtracePrt->max_frames = backtrace_zero.max_frames;
            backtracePrt->frame_size = backtrace_zero.frame_size;
            backtracePrt->frames = backtrace_zero.frames;

            wechat_backtrace::unwind_adapter(backtracePrt->frames.get(), backtracePrt->max_frames, backtracePrt->frame_size);
        }


        char *javaStack = get_java_stack();
        jstring java_stack = env->NewStringUTF(javaStack);

        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlGenBuffers, newArr, j_thread_id,
                                  java_stack, (int64_t) backtracePrt);

        delete[] result;
        free(javaStack);

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

        wechat_backtrace::Backtrace* backtracePrt;
        if (is_stacktrace_enabled) {
            wechat_backtrace::Backtrace backtrace_zero = BACKTRACE_INITIALIZER(MEMHOOK_BACKTRACE_MAX_FRAMES);

            backtracePrt = new wechat_backtrace::Backtrace;
            backtracePrt->max_frames = backtrace_zero.max_frames;
            backtracePrt->frame_size = backtrace_zero.frame_size;
            backtracePrt->frames = backtrace_zero.frames;

            wechat_backtrace::unwind_adapter(backtracePrt->frames.get(), backtracePrt->max_frames, backtracePrt->frame_size);
        }

        char *javaStack = get_java_stack();
        jstring java_stack = env->NewStringUTF(javaStack);

        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlGenFramebuffers, newArr,
                                  j_thread_id, java_stack, (int64_t) backtracePrt);

        delete[] result;
        free(javaStack);

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

//        vector<FrameData> *native_stacktrace = new std::vector<FrameData>;;
//        unwind_adapter(*native_stacktrace);
        int native_stacktrace = 0;

        char *javaStack = get_java_stack();
        jstring java_stack = env->NewStringUTF(javaStack);

        env->CallStaticVoidMethod(class_OpenGLHook, method_onGlGenRenderbuffers, newArr,
                                  j_thread_id, java_stack, (int64_t) native_stacktrace);

        delete[] result;
        free(javaStack);

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


#endif //OPENGL_API_HOOK_MY_FUNCTIONS_H