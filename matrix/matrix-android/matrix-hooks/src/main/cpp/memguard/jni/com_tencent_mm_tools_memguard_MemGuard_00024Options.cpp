//
// Created by tomystang on 2020/10/15.
//

#include <jni.h>
#include <cstdlib>
#include <util/Auxiliary.h>
#include <util/Log.h>
#include <jni/JNIAux.h>
#include <Options.h>
#include "com_tencent_mm_tools_memguard_MemGuard_00024Options.h"

using namespace memguard;

static jfieldID sFieldID_MemGuard$Options_maxAllocationSize = nullptr;
static jfieldID sFieldID_MemGuard$Options_maxDetectableAllocationCount = nullptr;
static jfieldID sFieldID_MemGuard$Options_maxSkippedAllocationCount = nullptr;
static jfieldID sFieldID_MemGuard$Options_percentageOfLeftSideGuard = nullptr;
static jfieldID sFieldID_MemGuard$Options_perfectRightSideGuard = nullptr;
static jfieldID sFieldID_MemGuard$Options_ignoreOverlappedReading = nullptr;
static jfieldID sFieldID_MemGuard$Options_issueDumpFilePath = nullptr;
static jfieldID sFieldID_MemGuard$Options_targetSOPatterns = nullptr;
static jfieldID sFieldID_MemGuard$Options_ignoredSOPatterns = nullptr;

#define PREPARE_FIELDID(env, var, clazz, name, sig, failure_ret_val) \
  do { \
    if (!(var)) { \
      (var) = (env)->GetFieldID(clazz, name, sig); \
      RETURN_ON_EXCEPTION(env, failure_ret_val); \
    } \
  } while (false)

bool memguard::jni::FillOptWithJavaOptions(JNIEnv *env, jobject jopts, Options* opts) {
    ASSERT(jopts != nullptr, "jopts is null.");
    ASSERT(opts != nullptr, "opts is null.");

    auto jopts_class = MakeScopedLocalRef(env->GetObjectClass(jopts));
    RETURN_ON_EXCEPTION(env, false);

    PREPARE_FIELDID(env, sFieldID_MemGuard$Options_maxAllocationSize, jopts_class.get(),
                    "maxAllocationSize", "I", false);
    opts->maxAllocationSize = env->GetIntField(jopts, sFieldID_MemGuard$Options_maxAllocationSize);
    RETURN_ON_EXCEPTION(env, false);

    PREPARE_FIELDID(env, sFieldID_MemGuard$Options_maxDetectableAllocationCount, jopts_class.get(),
                    "maxDetectableAllocationCount", "I", false);
    opts->maxDetectableAllocationCount = env->GetIntField(jopts, sFieldID_MemGuard$Options_maxDetectableAllocationCount);
    RETURN_ON_EXCEPTION(env, false);

    PREPARE_FIELDID(env, sFieldID_MemGuard$Options_maxSkippedAllocationCount, jopts_class.get(),
                    "maxSkippedAllocationCount", "I", false);
    opts->maxSkippedAllocationCount = env->GetIntField(jopts, sFieldID_MemGuard$Options_maxSkippedAllocationCount);
    RETURN_ON_EXCEPTION(env, false);

    PREPARE_FIELDID(env, sFieldID_MemGuard$Options_percentageOfLeftSideGuard, jopts_class.get(),
                    "percentageOfLeftSideGuard", "I", false);
    opts->percentageOfLeftSideGuard = env->GetIntField(jopts, sFieldID_MemGuard$Options_percentageOfLeftSideGuard);
    RETURN_ON_EXCEPTION(env, false);

    PREPARE_FIELDID(env, sFieldID_MemGuard$Options_perfectRightSideGuard, jopts_class.get(),
                    "perfectRightSideGuard", "Z", false);
    opts->perfectRightSideGuard = env->GetBooleanField(jopts, sFieldID_MemGuard$Options_perfectRightSideGuard);
    RETURN_ON_EXCEPTION(env, false);

    PREPARE_FIELDID(env, sFieldID_MemGuard$Options_ignoreOverlappedReading, jopts_class.get(),
                    "ignoreOverlappedReading", "Z", false);
    opts->ignoreOverlappedReading = env->GetBooleanField(jopts, sFieldID_MemGuard$Options_ignoreOverlappedReading);
    RETURN_ON_EXCEPTION(env, false);

    PREPARE_FIELDID(env, sFieldID_MemGuard$Options_issueDumpFilePath, jopts_class.get(),
                    "issueDumpFilePath", "Ljava/lang/String;", false);
    auto jIssueDumpFilePath = MakeScopedLocalRef(
          (jstring) env->GetObjectField(jopts, sFieldID_MemGuard$Options_issueDumpFilePath));
    RETURN_ON_EXCEPTION(env, false);
    if (jIssueDumpFilePath.get() != nullptr) {
        auto cIssueDumpFilePath = env->GetStringUTFChars(jIssueDumpFilePath.get(), nullptr);
        if (cIssueDumpFilePath != nullptr) {
            opts->issueDumpFilePath = cIssueDumpFilePath;
        }
        env->ReleaseStringUTFChars(jIssueDumpFilePath.get(), cIssueDumpFilePath);
    }

    PREPARE_FIELDID(env, sFieldID_MemGuard$Options_targetSOPatterns, jopts_class.get(),
                    "targetSOPatterns", "[Ljava/lang/String;", false);
    auto j_target_so_patterns_array = MakeScopedLocalRef(
          (jobjectArray) env->GetObjectField(jopts, sFieldID_MemGuard$Options_targetSOPatterns));
    RETURN_ON_EXCEPTION(env, false);
    if (j_target_so_patterns_array.get() != nullptr) {
        jint pattern_count = env->GetArrayLength(j_target_so_patterns_array.get());
        for (int i = 0; i < pattern_count; ++i) {
            auto j_pattern = MakeScopedLocalRef((jstring) env->GetObjectArrayElement(
                  j_target_so_patterns_array.get(), i));
            RETURN_ON_EXCEPTION(env, false);
            if (j_pattern.get() == nullptr) {
                continue;
            }
            jsize mutf_len = env->GetStringUTFLength(j_pattern.get());
            const char* mutf_pattern = env->GetStringUTFChars(j_pattern.get(), nullptr);
            opts->targetSOPatterns.emplace_back(std::string(mutf_pattern, mutf_len));
            env->ReleaseStringUTFChars(j_pattern.get(), mutf_pattern);
        }
    }

    PREPARE_FIELDID(env, sFieldID_MemGuard$Options_ignoredSOPatterns, jopts_class.get(),
                    "ignoredSOPatterns", "[Ljava/lang/String;", false);
    auto j_ignored_so_patterns_array = MakeScopedLocalRef(
          (jobjectArray) env->GetObjectField(jopts, sFieldID_MemGuard$Options_ignoredSOPatterns));
    RETURN_ON_EXCEPTION(env, false);
    if (j_ignored_so_patterns_array.get() != nullptr) {
        jint pattern_count = env->GetArrayLength(j_ignored_so_patterns_array.get());
        for (int i = 0; i < pattern_count; ++i) {
            auto j_pattern = MakeScopedLocalRef((jstring) env->GetObjectArrayElement(
                  j_ignored_so_patterns_array.get(), i));
            RETURN_ON_EXCEPTION(env, false);
            if (j_pattern.get() == nullptr) {
                continue;
            }
            jsize mutf_len = env->GetStringUTFLength(j_pattern.get());
            const char* mutf_pattern = env->GetStringUTFChars(j_pattern.get(), nullptr);
            opts->ignoredSOPatterns.emplace_back(std::string(mutf_pattern, mutf_len));
            env->ReleaseStringUTFChars(j_pattern.get(), mutf_pattern);
        }
    }

    return true;
}