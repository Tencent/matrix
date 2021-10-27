//
// Created by tomystang on 2021/2/1.
//

#include <jni.h>
#include <util/Log.h>
#include <unistd.h>
#include "JNIAux.h"
#include "C2Java.h"

using namespace memguard;

#define LOG_TAG "MemGuard.C2JAVA"

static jclass sMemGuardClazz = nullptr;
static jmethodID sMethodID_MemGuard_c2jNotifyOnIssueDumpped = nullptr;
static std::atomic_flag sInstalled(false);

#define PREPARE_METHODID(env, var, clazz, name, sig, is_static, failure_ret_val) \
  do { \
    if (!(var)) { \
      if (is_static) { \
        (var) = (env)->GetStaticMethodID(clazz, name, sig); \
      } else { \
        (var) = (env)->GetMethodID(clazz, name, sig); \
      } \
      RETURN_ON_EXCEPTION(env, failure_ret_val); \
    } \
  } while (false)

bool memguard::c2j::Prepare(jclass memguard_clazz) {
    if (sInstalled.test_and_set()) {
        LOGW(LOG_TAG, "Already prepared.");
        return true;
    }
    auto env = jni::GetEnv();
    sMemGuardClazz = (jclass) env->NewGlobalRef(memguard_clazz);
    PREPARE_METHODID(env, sMethodID_MemGuard_c2jNotifyOnIssueDumpped, memguard_clazz,
          "c2jNotifyOnIssueDumped", "(Ljava/lang/String;)V", true, false);
    return true;
}

void memguard::c2j::NotifyOnIssueDumpped(const char *dump_file) {
    auto env = jni::GetEnv();
    auto jDumpFile = env->NewStringUTF(dump_file);
    env->CallStaticVoidMethod(sMemGuardClazz, sMethodID_MemGuard_c2jNotifyOnIssueDumpped, jDumpFile);
}