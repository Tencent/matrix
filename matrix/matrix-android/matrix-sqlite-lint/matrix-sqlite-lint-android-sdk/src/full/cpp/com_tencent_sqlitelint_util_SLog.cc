/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "com_tencent_sqlitelint_util_SLog.h"
#include "jni_helper.h"

//
// Created by wuzhiwen on 2017/2/16.
//

namespace sqlitelint {

    static JavaVM *gJvm = nullptr;
    static jobject  gCallbackObj = nullptr;
    static jmethodID gCallbackLogMethod = nullptr;

    static int jniCallbackLogger(int priority, const char *msg) {
        assert(gCallbackObj && gCallbackLogMethod && gJvm);
        JNIEnv *env = nullptr;
        bool attached = false;
        jint ret = gJvm->GetEnv((void **) &env, JNI_VERSION_1_6);
        if (ret == JNI_EDETACHED) {
            jint ret = gJvm->AttachCurrentThread(&env, nullptr);
            assert(ret == JNI_OK);
            (void) ret;
            attached = true;
        }

        jstring tagStr = charsToJstring(env,SqliteLintTAG);
        jstring msgStr = charsToJstring(env,msg);
        env->CallVoidMethod(gCallbackObj, gCallbackLogMethod, priority, tagStr, msgStr);
        env->ExceptionClear();
        env->DeleteLocalRef(tagStr);
        env->DeleteLocalRef(msgStr);
        if (attached)
            gJvm->DetachCurrentThread();
        return 0;
    }

    extern "C" JNIEXPORT void Java_com_tencent_sqlitelint_util_SLog_nativeSetLogger(JNIEnv *env, jclass cls, jint logLevel) {
        if (logLevel <  kLevelVerbose || logLevel > kLevelNone){
            __android_log_print(ANDROID_LOG_ERROR,SqliteLintTAG,"logLevel err on nativeSetLogger, logLevel is %d",logLevel);
            return ;
        }
        SetSLogFunc(jniCallbackLogger);
        SetSLogLevel(SLogLevel(logLevel));
    }

    MODULE_INIT(sqliteLint){
        gJvm = vm;
        assert(env != nullptr);

        jclass callbackJavaClass = env->FindClass("com/tencent/sqlitelint/util/SLog");
        if (callbackJavaClass == nullptr){
            return -1;
        }

        gCallbackLogMethod = env->GetMethodID(callbackJavaClass, "printLog",
            "(ILjava/lang/String;Ljava/lang/String;)V");
        if (gCallbackLogMethod == nullptr){
            return -1;
        }
        jmethodID constructionMethodId = env->GetMethodID(callbackJavaClass, "<init>", "()V");
        jobject localCallbackObj = env->NewObject(callbackJavaClass, constructionMethodId);
        gCallbackObj = reinterpret_cast<jobject>(env->NewGlobalRef(localCallbackObj));
        return 0;
    }

    MODULE_FINI(sqliteLint){
        if (gCallbackObj)
            env->DeleteLocalRef(gCallbackObj);
        return 0;
    }
}
