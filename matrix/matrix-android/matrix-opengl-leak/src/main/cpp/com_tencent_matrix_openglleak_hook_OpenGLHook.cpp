//
// Created by 邓沛堆 on 2020-04-27.
//

#include <jni.h>
#include <android/log.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <cxxabi.h>
#include "com_tencent_matrix_openglleak_hook_OpenGLHook.h"
#include <string>
#include "get_tls.h"
#include "type.h"
#include "my_functions.h"

extern "C" JNIEXPORT jboolean JNICALL Java_com_tencent_matrix_openglleak_hook_OpenGLHook_init
        (JNIEnv *env, jobject thiz) {
    if (class_OpenGLHook == NULL) {
        // 暂存 class, hook 处只能用 bootclassloader
        class_OpenGLHook = env->FindClass("com/tencent/matrix/openglleak/hook/OpenGLHook");
        class_OpenGLHook = (jclass) env->NewGlobalRef(class_OpenGLHook);

        method_onGlGenTextures = env->GetStaticMethodID(class_OpenGLHook, "onGlGenTextures",
                                                        "([ILjava/lang/String;IJJ)V");
        method_onGlDeleteTextures = env->GetStaticMethodID(class_OpenGLHook, "onGlDeleteTextures",
                                                           "([ILjava/lang/String;J)V");
        method_onGlGenBuffers = env->GetStaticMethodID(class_OpenGLHook, "onGlGenBuffers",
                                                       "([ILjava/lang/String;IJJ)V");
        method_onGlDeleteBuffers = env->GetStaticMethodID(class_OpenGLHook, "onGlDeleteBuffers",
                                                          "([ILjava/lang/String;J)V");
        method_onGlGenFramebuffers = env->GetStaticMethodID(class_OpenGLHook, "onGlGenFramebuffers",
                                                            "([ILjava/lang/String;IJJ)V");
        method_onGlDeleteFramebuffers = env->GetStaticMethodID(class_OpenGLHook,
                                                               "onGlDeleteFramebuffers",
                                                               "([ILjava/lang/String;J)V");
        method_onGlGenRenderbuffers = env->GetStaticMethodID(class_OpenGLHook,
                                                             "onGlGenRenderbuffers",
                                                             "([ILjava/lang/String;IJJ)V");
        method_onGlDeleteRenderbuffers = env->GetStaticMethodID(class_OpenGLHook,
                                                                "onGlDeleteRenderbuffers",
                                                                "([ILjava/lang/String;J)V");
        method_onGetError = env->GetStaticMethodID(class_OpenGLHook, "onGetError", "(I)V");

        method_onGlBindTexture = env->GetStaticMethodID(class_OpenGLHook, "onGlBindTexture",
                                                        "(IIJ)V");
        method_onGlBindBuffer = env->GetStaticMethodID(class_OpenGLHook, "onGlBindBuffer",
                                                       "(IIJ)V");
        method_onGlBindFramebuffer = env->GetStaticMethodID(class_OpenGLHook, "onGlBindFramebuffer",
                                                            "(IIJ)V");
        method_onGlBindRenderbuffer = env->GetStaticMethodID(class_OpenGLHook,
                                                             "onGlBindRenderbuffer", "(IIJ)V");

        method_onGlTexImage2D = env->GetStaticMethodID(class_OpenGLHook, "onGlTexImage2D",
                                                       "(IIIIIIIIJIJJ)V");
        method_onGlTexImage3D = env->GetStaticMethodID(class_OpenGLHook, "onGlTexImage3D",
                                                       "(IIIIIIIIIJIJJ)V");
        method_onGlBufferData = env->GetStaticMethodID(class_OpenGLHook, "onGlBufferData",
                                                       "(IIJIJJ)V");
        method_onGlRenderbufferStorage = env->GetStaticMethodID(class_OpenGLHook,
                                                                "onGlRenderbufferStorage",
                                                                "(IIIIJIJJ)V");

        method_getThrowable = env->GetStaticMethodID(class_OpenGLHook, "getThrowable", "()I");

        messages_containers = new BufferManagement();
        messages_containers->start_process();
        pthread_key_create(&g_thread_name_key, [](void *thread_name) {
            if (thread_name != nullptr) {
                free(thread_name);
            }
        });
        pthread_key_create(&g_thread_id_key, [](void *thread_id) {
            if (thread_id != nullptr) {
                free(thread_id);
            }
        });

        return true;
    }

    return false;
}

extern "C" JNIEXPORT void JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_setNativeStackDump(JNIEnv *, jobject thiz,
                                                                      jboolean open) {
    enable_stacktrace(open);
}

extern "C" JNIEXPORT void JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_setJavaStackDump(JNIEnv *, jobject thiz,
                                                                    jboolean open) {
    enable_javastack(open);
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    m_java_vm = vm;
    return JNI_VERSION_1_6;
}


/*
 * Class:     com_tencent_matrix_openglleak_hook_OpenGLHook
 * Method:    hookGlGenTextures
 * Signature: (I)Z
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlGenTextures
        (JNIEnv *, jclass, jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glGenTextures = (System_GlNormal_TYPE) *origFunPtr;
    *origFunPtr = (void *) my_glGenTextures;

    return true;
}

/*
 * Class:     com_tencent_matrix_openglleak_hook_OpenGLHook
 * Method:    hookGlDeleteTextures
 * Signature: (I)Z
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlDeleteTextures
        (JNIEnv *, jclass, jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glDeleteTextures = (System_GlNormal_TYPE) *origFunPtr;
    *origFunPtr = (void *) my_glDeleteTextures;

    return true;
}

/*
 * Class:     com_tencent_matrix_openglleak_hook_OpenGLHook
 * Method:    hookGlGenBuffers
 * Signature: (I)Z
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlGenBuffers
        (JNIEnv *, jclass, jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glGenBuffers = (System_GlNormal_TYPE) *origFunPtr;
    *origFunPtr = (void *) my_glGenBuffers;

    return true;
}

/*
 * Class:     com_tencent_matrix_openglleak_hook_OpenGLHook
 * Method:    hookGlDeleteBuffers
 * Signature: (I)Z
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlDeleteBuffers
        (JNIEnv *, jclass, jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glDeleteBuffers = (System_GlNormal_TYPE) *origFunPtr;
    *origFunPtr = (void *) my_glDeleteBuffers;

    return true;
}

/*
 * Class:     com_tencent_matrix_openglleak_hook_OpenGLHook
 * Method:    hookGlGenFramebuffers
 * Signature: (I)Z
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlGenFramebuffers
        (JNIEnv *, jclass, jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glGenFramebuffers = (System_GlNormal_TYPE) *origFunPtr;
    *origFunPtr = (void *) my_glGenFramebuffers;

    return true;
}

/*
 * Class:     com_tencent_matrix_openglleak_hook_OpenGLHook
 * Method:    hookGlDeleteFramebuffers
 * Signature: (I)Z
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlDeleteFramebuffers
        (JNIEnv *, jclass, jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glDeleteFramebuffers = (System_GlNormal_TYPE) *origFunPtr;
    *origFunPtr = (void *) my_glDeleteFramebuffers;

    return true;
}

/*
 * Class:     com_tencent_matrix_openglleak_hook_OpenGLHook
 * Method:    hookGlGenRenderbuffers
 * Signature: (I)Z
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlGenRenderbuffers
        (JNIEnv *, jclass, jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glGenRenderbuffers = (System_GlNormal_TYPE) *origFunPtr;
    *origFunPtr = (void *) my_glGenRenderbuffers;

    return true;
}

/*
 * Class:     com_tencent_matrix_openglleak_hook_OpenGLHook
 * Method:    hookGlDeleteRenderbuffers
 * Signature: (I)Z
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlDeleteRenderbuffers
        (JNIEnv *, jclass, jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glDeleteRenderbuffers = (System_GlNormal_TYPE) *origFunPtr;
    *origFunPtr = (void *) my_glDeleteRenderbuffers;

    return true;
}

/*
 * Class:     com_tencent_matrix_openglleak_hook_OpenGLHook
 * Method:    hookGlGetError
 * Signature: (I)Z
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlGetError
        (JNIEnv *, jclass, jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glGetError = (System_GlGetError_TYPE) *origFunPtr;
    *origFunPtr = (void *) my_glGetError;

    return true;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlTexImage2D(JNIEnv *env, jclass clazz,
                                                                    jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glTexImage2D = (System_GlTexImage2D) *origFunPtr;
    *origFunPtr = (void *) my_glTexImage2D;

    return true;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlTexImage3D(JNIEnv *env, jclass clazz,
                                                                    jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glTexImage3D = (System_GlTexImage3D) *origFunPtr;
    *origFunPtr = (void *) my_glTexImage3D;

    return true;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlBindTexture(JNIEnv *env, jclass clazz,
                                                                     jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glBindTexture = (System_GlBind_TYPE) *origFunPtr;
    *origFunPtr = (void *) my_glBindTexture;

    return true;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlBindBuffer(JNIEnv *env, jclass clazz,
                                                                    jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glBindBuffer = (System_GlBind_TYPE) *origFunPtr;
    *origFunPtr = (void *) my_glBindBuffer;

    return true;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlBindFramebuffer(JNIEnv *env, jclass clazz,
                                                                         jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glBindFramebuffer = (System_GlBind_TYPE) *origFunPtr;
    *origFunPtr = (void *) my_glBindFramebuffer;

    return true;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlBindRenderbuffer(JNIEnv *env, jclass clazz,
                                                                          jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glBindRenderbuffer = (System_GlBind_TYPE) *origFunPtr;
    *origFunPtr = (void *) my_glBindRenderbuffer;

    return true;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlBufferData(JNIEnv *env, jclass clazz,
                                                                    jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glBufferData = (System_GlBufferData) *origFunPtr;
    *origFunPtr = (void *) my_glBufferData;

    return true;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_hookGlRenderbufferStorage(JNIEnv *env,
                                                                             jclass clazz,
                                                                             jint index) {
    gl_hooks_t *hooks = get_gl_hooks();
    if (NULL == hooks) {
        return false;
    }

    void **origFunPtr = NULL;

    origFunPtr = (void **) (&hooks->gl.foo1 + index);
    system_glRenderbufferStorage = (System_GlRenderbufferStorage) *origFunPtr;
    *origFunPtr = (void *) my_glRenderbufferStorage;

    return true;
}

void get_native_stack(wechat_backtrace::Backtrace *backtrace, char *&stack) {
    std::string caller_so_name;
    std::stringstream full_stack_builder;
    std::stringstream brief_stack_builder;
    std::string last_so_name;

    auto _callback = [&](wechat_backtrace::FrameDetail it) {
        std::string so_name = it.map_name;

        char *demangled_name;
        int status = 0;
        demangled_name = abi::__cxa_demangle(it.function_name, nullptr, 0, &status);

        full_stack_builder << "      | "
                           << "#pc " << std::hex << it.rel_pc << " "
                           << (demangled_name ? demangled_name : "(null)")
                           << " ("
                           << it.map_name
                           << ")"
                           << std::endl;

        if (last_so_name != it.map_name) {
            last_so_name = it.map_name;
            brief_stack_builder << it.map_name << ";";
        }

        brief_stack_builder << std::hex << it.rel_pc << ";";

        if (demangled_name) {
            free(demangled_name);
        }

        if (caller_so_name.empty()) { // fallback
            if (/*so_name.find("com.tencent.mm") == std::string::npos ||*/
                    so_name.find("libwechatbacktrace.so") != std::string::npos ||
                    so_name.find("libmatrix-hooks.so") != std::string::npos) {
                return;
            }
            caller_so_name = so_name;
        }
    };

    wechat_backtrace::restore_frame_detail(backtrace->frames.get(), backtrace->frame_size,
                                           _callback);

    stack = new char[full_stack_builder.str().size() + 1];
    strcpy(stack, full_stack_builder.str().c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_dumpNativeStack(JNIEnv *env, jclass clazz,
                                                                   jlong native_stack_ptr) {
    int64_t addr = native_stack_ptr;
    wechat_backtrace::Backtrace *ptr = (wechat_backtrace::Backtrace *) addr;

    char *native_stack = nullptr;
    get_native_stack(ptr, native_stack);

    jstring ret = env->NewStringUTF(native_stack);
    if (native_stack != nullptr) {
        free(native_stack);
    }
    return ret;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_tencent_matrix_openglleak_hook_OpenGLHook_releaseNative(JNIEnv *env, jclass clazz,
                                                                 jlong native_stack_ptr) {
    int64_t addr = native_stack_ptr;

    wechat_backtrace::Backtrace *ptr = (wechat_backtrace::Backtrace *) addr;
    delete ptr;
}