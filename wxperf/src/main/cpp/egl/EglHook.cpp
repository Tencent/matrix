//
// Created by 邓沛堆 on 2020-05-06.
//

#include "EglHook.h"
#include "log.h"
#include <EGL/egl.h>

#define ORIGINAL_LIB "libEGL.so"
#define TAG "EGL"

jstring charTojstring(JNIEnv *env, const char *pat) {
    //定义java String类 strClass
    jclass strClass = (env)->FindClass("java/lang/String");
    //获取String(byte[],String)的构造器,用于将本地byte[]数组转换为一个新String
    jmethodID ctorID = (env)->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V");
    //建立byte数组
    jbyteArray bytes = (env)->NewByteArray(strlen(pat));
    //将char* 转换为byte数组
    (env)->SetByteArrayRegion(bytes, 0, strlen(pat), (jbyte *) pat);
    // 设置String, 保存语言类型,用于byte数组转换至String时的参数
    jstring encoding = (env)->NewStringUTF("GB2312");
    //将byte数组转换为java String,并输出
    return (jstring) (env)->NewObject(strClass, ctorID, bytes, encoding);
}

void store_stack_info(uint64_t egl_context, char *java_stack, uint64_t native_stack_hash) {
    JNIEnv *env = NULL;
    if (m_java_vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        if (m_java_vm->AttachCurrentThread(&env, NULL) == JNI_OK) {
        } else {
            return;
        }
    }

    if (env != NULL) {
        jstring js = charTojstring(env, java_stack);
        env->CallStaticVoidMethod(m_class_EglHook,
                                  m_method_record,
                                  egl_context, native_stack_hash, js);
    }
}

void release_egl_context(uint64_t egl_context) {
    JNIEnv *env = NULL;
    if (m_java_vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        if (m_java_vm->AttachCurrentThread(&env, NULL) == JNI_OK) {
        } else {
            return;
        }
    }

    if (env != NULL) {
        env->CallStaticVoidMethod(m_class_EglHook,
                                  m_method_egl_release,
                                  egl_context);
    }
}

const size_t BUF_SIZE = 1024;

DEFINE_HOOK_FUN(EGLContext, eglCreateContext, EGLDisplay dpy, EGLConfig config,
                EGLContext share_context, const EGLint *attrib_list) {

    auto ptr_stack_frames = new std::vector<unwindstack::FrameData>;
    unwindstack::do_unwind(*ptr_stack_frames);
    uint64_t native_stack_hash_code = hash_stack_frames(*ptr_stack_frames);

    char *buf = static_cast<char *>(malloc(BUF_SIZE));
    if (buf) {
        get_java_stacktrace(buf, BUF_SIZE);
    }

    CALL_ORIGIN_FUNC_RET(EGLContext, ret, eglCreateContext, dpy, config, share_context,
                         attrib_list);

    store_stack_info((uint64_t) ret, buf, native_stack_hash_code);

    return ret;
}

DEFINE_HOOK_FUN(EGLBoolean, eglDestroyContext, EGLDisplay dpy, EGLContext ctx) {

    release_egl_context((uint64_t) ctx);

    CALL_ORIGIN_FUNC_RET(EGLBoolean, ret, eglDestroyContext, dpy, ctx);

    return ret;
}