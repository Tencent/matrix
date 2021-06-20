//
// Created by YinSheng Tang on 2021/6/19.
//

#include <jni.h>

#include "GCSemiSpaceTrimmer.h"

extern "C" jboolean JNIEXPORT
Java_com_tencent_matrix_hook_memory_GCSemiSpaceTrimmer_nativeInstall(JNIEnv* env, jobject) {
    return matrix::gc_ss_trimmer::Install(env);
}