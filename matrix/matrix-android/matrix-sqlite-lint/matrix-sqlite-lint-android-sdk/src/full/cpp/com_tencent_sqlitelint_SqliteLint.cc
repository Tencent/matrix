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
// Created by wuzhiwen on 2017/2/16.
//

#include "sqlite_lint.h"
#include "loader.h"
#include "jni_helper.h"
#include <string.h>

namespace sqlitelint {
    static JavaVM *kJvm;
    static jmethodID kExecSqlMethodID;
    static jobject kExecSqlObj;

    static jclass kJavaBridgeClass;
    static jclass kIssueClass;

    static jmethodID kListConstruct;
    static jmethodID kListAdd;

    static jmethodID kMethodIDIssueConstruct;
    static jmethodID kMethodIDOnPublishIssueCallback;

    int SqliteLintExecSql(const char *file_name, const char *sql, SqlExecutionCallback callback,
                          void *para, char **err_msg) {
        int64_t exec_sql_callback_ptr = (int64_t) (intptr_t) callback;
        int64_t para_ptr = (int64_t) (intptr_t) para;

        if (kExecSqlMethodID == nullptr) {
            LOGE("sqliteLintExecSql gExecSqlMethodID is null");
            return -1;
        }

        if (kExecSqlObj == nullptr) {
            LOGE("sqliteLintExecSql gExecSqlObj is null");
            return -1;
        }

        JNIEnv *env = nullptr;
        bool attached = false;
        jint ret = kJvm->GetEnv((void **) &env, JNI_VERSION_1_6);
        if (ret == JNI_EDETACHED) {
            jint ret = kJvm->AttachCurrentThread(&env, nullptr);
            assert(ret == JNI_OK);
            (void) ret;
            attached = true;
        }

        jstring filename_str = charsToJstring(env,file_name);
        jstring sql_str = charsToJstring(env,sql);
        jobjectArray retArray = (jobjectArray) env->CallObjectMethod(kExecSqlObj, kExecSqlMethodID,filename_str, sql_str,
                                                                     callback ? JNI_TRUE : JNI_FALSE, exec_sql_callback_ptr,para_ptr);
        env->DeleteLocalRef(filename_str);
        env->DeleteLocalRef(sql_str);
        if (!retArray) {
            LOGE("sqliteLintExecSql retArray is null");
            if (attached) kJvm->DetachCurrentThread();
            return -1;
        }

        jsize len = env->GetArrayLength(retArray);
        if (len >= 2) {
            jstring msg = (jstring) env->GetObjectArrayElement(retArray, 0);
            jstring rc_str = (jstring) env->GetObjectArrayElement(retArray, 1);
            (*err_msg) = jstringToChars(env,msg);
            char *rc_char = jstringToChars(env,rc_str);
            int rc = atoi(rc_char);
            free(rc_char);
            env->DeleteLocalRef(msg);
            env->DeleteLocalRef(rc_str);
            if (rc != 0) {
                LOGI("sqliteLintExecSql retArray rc is %d", rc);
            }
            if (attached) kJvm->DetachCurrentThread();
            return rc;
        }

        if (attached) kJvm->DetachCurrentThread();

        LOGI("sqliteLintExecSql retArray len is %d", len);

        return -1;
    }

    extern "C" {
    void Java_com_tencent_sqlitelint_SQLiteLintNativeBridge_execSqlCallback(JNIEnv *env, jobject, jlong exec_sql_callback_ptr, jlong para_ptr, jstring name, jint n_column,
                                                                jobjectArray column_value, jobjectArray column_name) {
        if (!n_column) {
            return;
        }
        SqlExecutionCallback exec_sql_callback = (SqlExecutionCallback) (intptr_t) exec_sql_callback_ptr;
        if (!exec_sql_callback) {
            LOGE("execSqlCallback execSqlCallback is NULL");
            return;
        }
        void* para = (void *) (intptr_t) para_ptr;
        if (!para) {
            LOGE("execSqlCallback para is NULL");
            return;
        }

        jstring jstr;
        char **p_column_value = (char **) malloc(n_column * sizeof(char *));
        char **p_column_name = (char **) malloc(n_column * sizeof(char *));
        int i = 0;
        for (i = 0; i < n_column; i++) {
            jstr = (jstring) env->GetObjectArrayElement(column_value, i);
            p_column_value[i] = jstringToChars(env,jstr);
            env->DeleteLocalRef(jstr);
        }
        for (i = 0; i < n_column; i++) {
            jstr = (jstring) env->GetObjectArrayElement(column_name, i);
            p_column_name[i] = jstringToChars(env,jstr);
            env->DeleteLocalRef(jstr);
        }

        char *filename = jstringToChars(env,name);
        if(nullptr == filename){
            filename = (char *) "";
        }
        exec_sql_callback(para, n_column, p_column_value, p_column_name);
        if(strlen(filename)) free(filename);

        //free
        for (i = 0; i < n_column; i++) {
            free(p_column_value[i]);
            free(p_column_name[i]);
        }
        free(p_column_value);
        free(p_column_name);
    }

    void OnIssuePublish(const char* db_path, std::vector<Issue> published_issues) {
        if (kJavaBridgeClass == nullptr) {
            LOGE("OnIssuePublish kJavaBridgeClass is null");
            return;
        }

        if (kMethodIDOnPublishIssueCallback == nullptr) {
            LOGE("OnIssuePublish kMethodIDOnPublishIssueCallback is null");
            return;
        }

        if (kListConstruct == nullptr) {
            LOGE("OnIssuePublish kListConstruct is null");
            return;
        }

        if (kListAdd == nullptr) {
            LOGE("OnIssuePublish kListAdd is null");
            return;
        }

        bool attached = false;
        JNIEnv* env = nullptr;
        jint jRet = kJvm->GetEnv((void **) &env, JNI_VERSION_1_6);
        if (jRet == JNI_EDETACHED) {
            LOGD("OnIssuePublish GetEnv JNI_EDETACHED");
            jint jAttachRet = kJvm->AttachCurrentThread(&env, nullptr);
            if (jAttachRet != JNI_OK) {
                LOGE("OnIssuePublish AttachCurrentThread !JNI_OK");
                return;
            } else {
                attached = true;
            }
        } else if (jRet != JNI_OK) {
            LOGE("OnIssuePublish GetEnv !JNI_OK");
            return;
        }

        LOGV("OnIssuePublish issue size %zu", published_issues.size());

        jclass listCls = env->FindClass("java/util/ArrayList");

        jobject jIssueList = env->NewObject(listCls, kListConstruct);

        for (std::vector<Issue>::iterator it = published_issues.begin(); it != published_issues.end(); it++) {
            jstring id = charsToJstring(env,it->id.c_str());
            jstring dbPath = charsToJstring(env,it->db_path.c_str());
            jint level = it->level;
            jint type = it->type;
            jstring sql = charsToJstring(env,it->sql.c_str());
            jstring table = charsToJstring(env,it->table.c_str());
            jstring desc = charsToJstring(env,it->desc.c_str());
            jstring detail = charsToJstring(env,it->detail.c_str());
            jstring advice = charsToJstring(env,it->advice.c_str());
            jlong createTime = it->create_time;
            jstring extInfo = charsToJstring(env, it->ext_info.c_str());
            jlong sqlTimeCost = it->sql_time_cost;
            jboolean isInMainThread = it->is_in_main_thread;

            jobject diagnosisObj = env->NewObject(kIssueClass, kMethodIDIssueConstruct, id, dbPath,
                                                  level, type, sql, table, desc, detail, advice, createTime, extInfo, sqlTimeCost, isInMainThread);
            LOGV("OnIssuePublish id=%s", it->id.c_str());

            env->CallBooleanMethod(jIssueList, kListAdd, diagnosisObj);
            env->DeleteLocalRef(id);
            env->DeleteLocalRef(dbPath);
            env->DeleteLocalRef(sql);
            env->DeleteLocalRef(table);
            env->DeleteLocalRef(desc);
            env->DeleteLocalRef(detail);
            env->DeleteLocalRef(advice);
            env->DeleteLocalRef(extInfo);
        }

        jstring jDbPath = charsToJstring(env, db_path);
        env->CallStaticVoidMethod(kJavaBridgeClass, kMethodIDOnPublishIssueCallback, jDbPath, jIssueList);

        env->DeleteLocalRef(jDbPath);

        if (attached) kJvm->DetachCurrentThread();
    }

    void Java_com_tencent_sqlitelint_SQLiteLintNativeBridge_nativeInstall(JNIEnv *env, jobject, jstring name) {
        char *filename = jstringToChars(env,name);
        InstallSQLiteLint(filename, OnIssuePublish);
        free(filename);
        SetSqlExecutionDelegate(SqliteLintExecSql);
    }

    void Java_com_tencent_sqlitelint_SQLiteLintNativeBridge_nativeUninstall(JNIEnv *env, jobject, jstring name) {
        char *filename = jstringToChars(env,name);
        UninstallSQLiteLint(filename);
        free(filename);
    }

    void Java_com_tencent_sqlitelint_SQLiteLintNativeBridge_nativeNotifySqlExecute(JNIEnv *env, jobject, jstring dbPath
            , jstring sql, jlong executeTime, jstring extInfo) {
        char *filename = jstringToChars(env, dbPath);
        char *ext_info = jstringToChars(env, extInfo);
        char *jsql = jstringToChars(env, sql);

        NotifySqlExecution(filename, jsql, executeTime, ext_info);

        free(jsql);
        free(ext_info);
        free(filename);
    }

    void Java_com_tencent_sqlitelint_SQLiteLintNativeBridge_nativeAddToWhiteList(JNIEnv *env, jobject, jstring db_path, jobjectArray check_name_arr, jobjectArray white_list_arr) {
        std::map<std::string, std::set<std::string>> whiteList;

        jint j_check_name_arr_count = env->GetArrayLength(check_name_arr);
        for (jint i = 0 ; i < j_check_name_arr_count ; i++) {
            jstring j_checker_name = (jstring) env->GetObjectArrayElement(check_name_arr, i);
            char* checkerName = jstringToChars(env, j_checker_name);

            if (whiteList.find(checkerName) == whiteList.end()) {
                whiteList[checkerName] = std::set<std::string>();
            }

            jobjectArray j_white_list_arr = (jobjectArray) env->GetObjectArrayElement(white_list_arr, i);
            jint j_white_list_arr_count = env->GetArrayLength(j_white_list_arr);
            for (jint j = 0; j < j_white_list_arr_count ; j++) {
                jstring jElement = (jstring) env->GetObjectArrayElement(j_white_list_arr, j);
                char* element = jstringToChars(env, jElement);

                whiteList[checkerName].insert(element);

                free(element);
            }

            free(checkerName);
        }

        char *dbPath = jstringToChars(env, db_path);
        SetWhiteList(dbPath, whiteList);
        free(dbPath);
    }

    void Java_com_tencent_sqlitelint_SQLiteLintNativeBridge_nativeEnableCheckers(JNIEnv *env, jobject, jstring dbPathStr, jobjectArray enableCheckerArr) {
        char *dbPath = jstringToChars(env, dbPathStr);

        jint j_check_name_arr_count = env->GetArrayLength(enableCheckerArr);
        for (jint i = 0 ; i < j_check_name_arr_count ; i++) {
            jstring j_checker_name = (jstring) env->GetObjectArrayElement(enableCheckerArr, i);
            char *checkerName = jstringToChars(env, j_checker_name);
            EnableChecker(dbPath, checkerName);
            free(checkerName);
        }

        free(dbPath);
    }

    MODULE_INIT(sqliteLint) {
        kJvm = vm;
        assert(env != nullptr);

        jclass tmp_java_bridge_class = env->FindClass("com/tencent/sqlitelint/SQLiteLintNativeBridge");
        if (tmp_java_bridge_class == nullptr) {
            LOGE("MODULE_INIT tmpJavaBridgeClass is null");
            return -1;
        }
        kJavaBridgeClass = reinterpret_cast<jclass>(env->NewGlobalRef(tmp_java_bridge_class));

        kExecSqlMethodID = env->GetMethodID(kJavaBridgeClass, "sqliteLintExecSql",
                                            "(Ljava/lang/String;Ljava/lang/String;ZJJ)[Ljava/lang/String;");
        if (kExecSqlMethodID == nullptr) {
            LOGE("MODULE_INIT gExecSqlMethodID is null");
            return -1;
        }

        jmethodID construction_method_id = env->GetMethodID(kJavaBridgeClass, "<init>", "()V");
        jobject local_callback_obj = env->NewObject(kJavaBridgeClass, construction_method_id);
        kExecSqlObj = reinterpret_cast<jobject>(env->NewGlobalRef(local_callback_obj));
        env->DeleteLocalRef(local_callback_obj);

        kMethodIDOnPublishIssueCallback = env->GetStaticMethodID(kJavaBridgeClass, "onPublishIssue", "(Ljava/lang/String;Ljava/util/ArrayList;)V");
        if (kMethodIDOnPublishIssueCallback == nullptr) {
            LOGE("MODULE_INIT kMethodIDOnPublishIssueCallback is null");
            return -1;
        }

        jclass tmp_issue_class = env->FindClass("com/tencent/sqlitelint/SQLiteLintIssue");
        if (tmp_issue_class == nullptr) {
            LOGE("MODULE_INIT tmpIssueClass is null");
            return -1;
        }
        kIssueClass = reinterpret_cast<jclass>(env->NewGlobalRef(tmp_issue_class));

        kMethodIDIssueConstruct = env->GetMethodID(kIssueClass, "<init>",
                                                     "(Ljava/lang/String;Ljava/lang/String;IILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;JLjava/lang/String;JZ)V");
        jclass list_cls = env->FindClass("java/util/ArrayList");
        kListConstruct = env->GetMethodID(list_cls, "<init>", "()V");
        kListAdd = env->GetMethodID(list_cls, "add", "(Ljava/lang/Object;)Z");
        if (kMethodIDIssueConstruct == nullptr) {
            LOGE("MODULE_INIT kMethodIDIssueConstruct is null");
            return -1;
        }
        return 0;
    }

    MODULE_FINI(sqliteLint) {
        if (kExecSqlObj)
            env->DeleteLocalRef(kExecSqlObj);
        if (kIssueClass)
            env->DeleteGlobalRef(kIssueClass);
        if (kJavaBridgeClass)
            env->DeleteGlobalRef(kJavaBridgeClass);
        return 0;
    }
    }//end of extern "C"
}//end of namespace
