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

package com.tencent.matrix.resource.hproflib;

/**
 * Created by tangyinsheng on 2017/6/25.
 */

public final class HprofConstants {
    public static final int RECORD_TAG_UNKNOWN = 0x0;
    public static final int RECORD_TAG_STRING = 0x1;
    public static final int RECORD_TAG_LOAD_CLASS = 0x2;
    public static final int RECORD_TAG_UNLOAD_CLASS = 0x3;
    public static final int RECORD_TAG_STACK_FRAME = 0x4;
    public static final int RECORD_TAG_STACK_TRACE = 0x5;
    public static final int RECORD_TAG_ALLOC_SITES = 0x6;
    public static final int RECORD_TAG_HEAP_SUMMARY = 0x7;
    public static final int RECORD_TAG_START_THREAD = 0xa;
    public static final int RECORD_TAG_END_THREAD = 0xb;
    public static final int RECORD_TAG_HEAP_DUMP = 0xc;
    public static final int RECORD_TAG_HEAP_DUMP_SEGMENT = 0x1c;
    public static final int RECORD_TAG_HEAP_DUMP_END = 0x2c;
    public static final int RECORD_TAG_CPU_SAMPLES = 0xd;
    public static final int RECORD_TAG_CONTROL_SETTINGS = 0xe;

    public static final int HEAPDUMP_ROOT_UNKNOWN = 0xff;

    public static final int HEAPDUMP_ROOT_JNI_GLOBAL = 0x1;
    public static final int HEAPDUMP_ROOT_JNI_LOCAL = 0x2;
    public static final int HEAPDUMP_ROOT_JAVA_FRAME = 0x3;
    public static final int HEAPDUMP_ROOT_NATIVE_STACK = 0x4;
    public static final int HEAPDUMP_ROOT_STICKY_CLASS = 0x5;
    public static final int HEAPDUMP_ROOT_THREAD_BLOCK = 0x6;
    public static final int HEAPDUMP_ROOT_MONITOR_USED = 0x7;
    public static final int HEAPDUMP_ROOT_THREAD_OBJECT = 0x8;
    public static final int HEAPDUMP_ROOT_CLASS_DUMP = 0x20;
    public static final int HEAPDUMP_ROOT_INSTANCE_DUMP = 0x21;
    public static final int HEAPDUMP_ROOT_OBJECT_ARRAY_DUMP = 0x22;
    public static final int HEAPDUMP_ROOT_PRIMITIVE_ARRAY_DUMP = 0x23;
    /* Android tags */
    public static final int HEAPDUMP_ROOT_HEAP_DUMP_INFO = 0xfe;
    public static final int HEAPDUMP_ROOT_INTERNED_STRING = 0x89;
    public static final int HEAPDUMP_ROOT_FINALIZING = 0x8a;
    public static final int HEAPDUMP_ROOT_DEBUGGER = 0x8b;
    public static final int HEAPDUMP_ROOT_REFERENCE_CLEANUP = 0x8c;
    public static final int HEAPDUMP_ROOT_VM_INTERNAL = 0x8d;
    public static final int HEAPDUMP_ROOT_JNI_MONITOR = 0x8e;
    public static final int HEAPDUMP_ROOT_UNREACHABLE = 0x90;  /* deprecated */
    public static final int HEAPDUMP_ROOT_PRIMITIVE_ARRAY_NODATA_DUMP = 0xc3;

    private HprofConstants() {
        throw new UnsupportedOperationException();
    }
}
