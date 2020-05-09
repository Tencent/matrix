//
// Created by Yves on 2020-03-16.
//
#include "JNICommon.h"
#include "Log.h"
#include "HookCommon.h"
#include "xhook.h"

#ifdef __cplusplus
extern "C" {
#endif

JavaVM *m_java_vm;

jclass m_class_HookManager;
jmethodID m_method_getStack;

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGD("Yves-debug", "JNI OnLoad...");
    m_java_vm = vm;

    JNIEnv *env;

    vm->GetEnv((void **)&env, JNI_VERSION_1_6);

    if (env) {
        jclass j_HookManager = env->FindClass("com/tencent/mm/performance/jni/HookManager");

        if (j_HookManager) {
            LOGD("Yves-debug", "j_PthreadHook not null");
            m_class_HookManager = (jclass) env->NewGlobalRef(j_HookManager);
            m_method_getStack = env->GetStaticMethodID(m_class_HookManager, "getStack", "()Ljava/lang/String;");
        } else {
            LOGD("Yves-debug", "j_PthreadHook null!");
        }
    }

    xhook_ignore(".*libwxperf\\.so$", NULL);
    xhook_ignore(".*liblog\\.so$", NULL);
    xhook_ignore(".*libc\\.so$", NULL);
    xhook_ignore(".*libm\\.so$", NULL);
    xhook_ignore(".*libc\\+\\+\\.so$", NULL);
    xhook_ignore(".*libc\\+\\+_shared\\.so$", NULL);
    xhook_ignore(".*libstdc\\+\\+.so\\.so$", NULL);
    xhook_ignore(".*libstlport_shared\\.so$", NULL);

    xhook_register(".*\\.so$", "__loader_android_dlopen_ext", (void *) HANDLER_FUNC_NAME(__loader_android_dlopen_ext),
                   (void **) &ORIGINAL_FUNC_NAME(__loader_android_dlopen_ext));

    return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM* vm, void* reserved) {

}

#ifdef __cplusplus
}
#endif