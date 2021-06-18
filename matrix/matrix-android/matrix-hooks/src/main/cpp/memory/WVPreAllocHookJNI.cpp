//
// Created by YinSheng Tang on 2021/5/12.
//

#include <jni.h>
#include "WVPreAllocTrimmer.h"

using namespace matrix;

extern "C" jboolean JNIEXPORT Java_com_tencent_matrix_hook_memory_WVPreAllocHook_installHooksNative(JNIEnv* env, jobject thiz,
        jint sdk_ver, jobject class_loader, jboolean enable_debug) {
    return wv_prealloc_trimmer::Install(env, sdk_ver, class_loader) ? JNI_TRUE : JNI_FALSE;
}