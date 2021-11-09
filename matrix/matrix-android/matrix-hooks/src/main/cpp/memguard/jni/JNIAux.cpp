//
// Created by tomystang on 2020/11/18.
//

#include <unistd.h>
#include <util/Auxiliary.h>
#include <util/Log.h>
#include <type_traits>
#include "JNIAux.h"

using namespace memguard;

#define LOG_TAG "MemGuard.JNIAux"

static JavaVM* sVM;
static pthread_once_t sInitOnceKey;
static pthread_key_t sAttachFlagTLSKey;

static jclass sClass_Throwable = nullptr;
static jmethodID sMethodID_printStackTrace = nullptr;

bool memguard::jni::InitializeEnv(JavaVM* vm) {
    sVM = vm;

    auto env = GetEnv();
    sClass_Throwable = (jclass) env->NewGlobalRef(env->FindClass("java/lang/Throwable"));
    RETURN_ON_EXCEPTION(env, false);
    sMethodID_printStackTrace = env->GetMethodID(sClass_Throwable, "printStackTrace", "()V");
    RETURN_ON_EXCEPTION(env, false);

    LOGD(LOG_TAG, "InitializeEnv done.");
    return true;
}

JNIEnv* memguard::jni::GetEnv() {
    ASSERT(sVM != nullptr, "Not initialized.");
    JNIEnv* result = nullptr;
    auto ret = sVM->GetEnv((void**) &result, JNI_VERSION_1_6);
    ASSERT(ret != JNI_EVERSION, "System is too old to be supported.");
    if (ret != JNI_OK) {
        pthread_once(&sInitOnceKey, []() {
            pthread_key_create(&sAttachFlagTLSKey, [](void* flag) {
                if (flag) {
                    sVM->DetachCurrentThread();
                }
            });
        });
        if (sVM->AttachCurrentThread(&result, nullptr) == JNI_OK) {
            pthread_setspecific(sAttachFlagTLSKey, (void*) 1);
        } else {
            result = nullptr;
        }
    }
    ASSERT(result != nullptr, "Fail to get JNIEnv on current thread (%d).", gettid());
    return result;
}

void memguard::jni::PrintStackTrace(JNIEnv *env, jthrowable thr) {
    env->CallVoidMethod(thr, sMethodID_printStackTrace);
}