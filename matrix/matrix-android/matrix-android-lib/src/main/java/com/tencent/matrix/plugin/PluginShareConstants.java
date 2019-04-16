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

package com.tencent.matrix.plugin;

/**
 * Created by zhouzhijie on 2019/3/13.
 */
public class PluginShareConstants {
    public static class MemoryCanaryShareKeys {
        public static final String SYSTEM_MEMORY = "sysMem";
        public static final String MEM_CLASS = "memClass";
        public static final String AVAILABLE = "available";
        public static final String DALVIK_HEAP = "dalvikHeap";
        public static final String NATIVE_HEAP = "nativeHeap";
        public static final String MEM_FREE = "memfree";
        public static final String IS_LOW = "islow";
        public static final String VM_SIZE = "vmSize";
    }
}
