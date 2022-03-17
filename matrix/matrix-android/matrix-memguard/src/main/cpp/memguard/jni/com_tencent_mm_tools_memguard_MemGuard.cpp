//
// Created by tomystang on 2020/10/15.
//

#include <jni.h>
#include <util/Auxiliary.h>
#include <util/Hook.h>
#include <util/Unwind.h>
#include <util/Log.h>
#include <Options.h>
#include <cstring>
#include <MemGuard.h>
#include "JNIAux.h"
#include "com_tencent_mm_tools_memguard_MemGuard_00024Options.h"
#include "C2Java.h"

using namespace memguard;

#define LOG_TAG "MemGuard.JNI"

#define JNI_CLASS_PREFIX com_tencent_matrix_memguard_MemGuard

extern "C" jint JNI_OnLoad(JavaVM* vm, void*) {
    if (!jni::InitializeEnv(vm)) {
        LOGE(LOG_TAG, "Fail to initialize jni environment.");
        return JNI_ERR;
    }
    LOGI(LOG_TAG, "Libmemguard was loaded.");
    return JNI_VERSION_1_6;
}

extern "C" jboolean JNI_METHOD(nativeInstall)(JNIEnv* env, jclass memguard_clazz, jobject opts) {
    Options raw_opts;
    jni::FillOptWithJavaOptions(env, opts, &raw_opts);
    c2j::Prepare(memguard_clazz);
    if (Install(&raw_opts)) {
        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }
}

extern "C" jstring JNI_METHOD(nativeGetIssueDumpFilePath)(JNIEnv* env, jclass) {
    if (gOpts.issueDumpFilePath.empty()) {
        return nullptr;
    }
    return env->NewStringUTF(gOpts.issueDumpFilePath.c_str());
}