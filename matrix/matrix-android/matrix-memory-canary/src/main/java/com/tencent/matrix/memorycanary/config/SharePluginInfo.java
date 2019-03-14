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

package com.tencent.matrix.memorycanary.config;

/**
 * @author zhouzhijie
 * Created by zhouzhijie on 2018/09.
 */

public class SharePluginInfo {
    public static final String TAG_PLUGIN = "memoryinfo";


    public static final class IssueType {
        public static final int ISSUE_TRIM = 0x1;
        public static final int ISSUE_INFO = 0x2;
    }

    public static final String ISSUE_SYSTEM_MEMORY = "sysMem";
    public static final String ISSUE_THRESHOLD = "threshold";
    public static final String ISSUE_MEM_CLASS = "memClass";
    public static final String ISSUE_AVAILABLE = "available";
    public static final String ISSUE_APP_PSS = "pss";
    public static final String ISSUE_APP_JAVA = "java";
    public static final String ISSUE_APP_NATIVE = "native";
    public static final String ISSUE_APP_GRAPHICS = "graphics";
    public static final String ISSUE_APP_STACK = "stack";
    public static final String ISSUE_APP_CODE = "code";
    public static final String ISSUE_APP_OTHER = "other";
    public static final String ISSUE_ACTIVITY = "activity";
    public static final String ISSUE_FOREGROUND = "front";
    public static final String ISSUE_STARTED_TIME = "span";
    public static final String ISSUE_APP_MEM = "appmem";
    public static final String ISSUE_TRIM_FLAG = "trimFlag";
    public static final String ISSUE_DALVIK_HEAP = "dalvikHeap";
    public static final String ISSUE_NATIVE_HEAP = "nativeHeap";
    public static final String ISSUE_APP_USS = "uss";
    public static final String ISSUE_MEM_FREE = "memfree";
    public static final String ISSUE_IS_LOW = "islow";
}
