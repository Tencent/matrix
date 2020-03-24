//
// Created by Yves on 2020-03-16.
//
#include "JNICommon.h"
#include "log.h"
#include "HookCommon.h"
#include "xhook.h"

#ifdef __cplusplus
extern "C" {
#endif

JavaVM *m_java_vm;

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGD("Yves-debug", "JNI OnLoad...");
    m_java_vm = vm;

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