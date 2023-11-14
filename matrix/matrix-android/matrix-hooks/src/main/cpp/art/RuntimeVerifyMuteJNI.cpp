//
// Created by tomystang on 2022/11/17.
//

#include <jni.h>
#include "RuntimeVerifyMute.h"

extern "C" jboolean JNIEXPORT
Java_com_tencent_matrix_hook_art_RuntimeVerifyMute_nativeInstall(JNIEnv* env, jobject) {
    return matrix::art_misc::Install(env) ? JNI_TRUE : JNI_FALSE;
}