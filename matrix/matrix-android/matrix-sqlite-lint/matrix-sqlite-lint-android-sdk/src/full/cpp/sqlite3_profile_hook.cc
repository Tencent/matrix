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

//
// hook sqlite3_profile
//
// @author liyongjie
// Created by liyongjie on 2017/8/8.
//

#include <jni.h>
#include <xhook.h>
#include "sqlite_lint.h"
#include "lemon/sqlite3.h"
#include "com_tencent_sqlitelint_util_SLog.h"
#include "jni_helper.h"

namespace sqlitelint {

    static bool kInitSuc = false;
    static bool kStop = false;
    static JavaVM *kJvm;
    static jclass kUtilClass;
    static jmethodID kMethodIDGetThrowableStack;

    static void* (*original_sqlite3_profile) (sqlite3* db, void(*xProfile)(void*, const char*, sqlite_uint64), void* p);

    extern "C" {

    struct SQLiteConnection {
        sqlite3* const db;
        const int openFlags;
        const char*  path;
        const char* label;

        volatile bool canceled;

        SQLiteConnection(sqlite3* db, int openFlags, const char*  path, const char*  label) :
                db(db), openFlags(openFlags), path(path), label(label), canceled(false) { }
    };

    static JNIEnv* getJNIEnv() {
        //ensure kJvm init
        assert(kJvm != nullptr);

        JNIEnv* env = nullptr;
        if (kJvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK){
            LOGE("getJNIEnv !JNI_OK");
        }

        return env;
    }

    static void SQLiteLintSqlite3ProfileCallback(void *data, const char *sql, sqlite_uint64 tm) {
        if (kStop) {
            return;
        }

        JNIEnv*env = getJNIEnv();
        if (env != nullptr) {
            SQLiteConnection* connection = static_cast<SQLiteConnection*>(data);
            jstring extInfo = (jstring)env->CallStaticObjectMethod(kUtilClass, kMethodIDGetThrowableStack);
            char *ext_info = jstringToChars(env, extInfo);

            NotifySqlExecution(connection->label, sql, tm/1000000, ext_info);

            free(ext_info);
        } else {
            LOGW("SQLiteLintSqlite3ProfileCallback env null");
        }
    }


    void* hooked_sqlite3_profile(sqlite3* db, void(*xProfile)(void*, const char*, sqlite_uint64), void* p) {
        LOGI("hooked_sqlite3_profile call");
        return original_sqlite3_profile(db, SQLiteLintSqlite3ProfileCallback, p);
    }

    JNIEXPORT jboolean JNICALL Java_com_tencent_sqlitelint_util_SQLite3ProfileHooker_nativeDoHook(JNIEnv *env, jobject /* this */) {
        LOGI("SQLiteLintHooker_nativeDoHook");
        if (!kInitSuc) {
            LOGW("SQLiteLintHooker_nativeDoHook kInitSuc failed");
            return false;
        }
        xhook_register(".*/libandroid_runtime\\.so$", "sqlite3_profile", (void*)hooked_sqlite3_profile, (void**)&original_sqlite3_profile);

        #ifndef NDEBUG
        xhook_enable_sigsegv_protection(0);
        #else
        xhook_enable_sigsegv_protection(1);
        #endif

        xhook_refresh(0);

        kStop = false;

        return true;
    }

    JNIEXPORT jboolean JNICALL Java_com_tencent_sqlitelint_util_SQLite3ProfileHooker_nativeStartProfile(JNIEnv *env, jobject /* this */) {
        LOGI("SQLiteLintHooker_nativeStartProfile");
        kStop = false;
        return true;

    }

    JNIEXPORT jboolean JNICALL Java_com_tencent_sqlitelint_util_SQLite3ProfileHooker_nativeStopProfile(JNIEnv *env, jobject /* this */) {
        LOGI("SQLiteLintHooker_nativeStopProfile");
        kStop = true;
        return true;

    }

    MODULE_INIT(sqlite3_proifle_hook){
        kInitSuc = false;
        kJvm = vm;
        jint result = -1;

        jclass utilClass = env->FindClass("com/tencent/sqlitelint/util/SQLiteLintUtil");
        if (utilClass == nullptr)  {
            return result;
        }
        kUtilClass = reinterpret_cast<jclass>(env->NewGlobalRef(utilClass));
        kMethodIDGetThrowableStack = env->GetStaticMethodID(kUtilClass, "getThrowableStack", "()Ljava/lang/String;");
        if (kMethodIDGetThrowableStack == nullptr) {
            return result;
        }

        kInitSuc = true;
        return 0;
    }

    MODULE_FINI(sqlite3_proifle_hook) {
        if (kUtilClass) {
            env->DeleteGlobalRef(kUtilClass);
        }
        kInitSuc = false;
        kStop = true;
        return 0;
    }
}
}
