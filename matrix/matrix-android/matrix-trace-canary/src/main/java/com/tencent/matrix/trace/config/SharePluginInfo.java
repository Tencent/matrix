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

package com.tencent.matrix.trace.config;

/**
 * Created by zhangshaowen on 17/7/13.
 */

public class SharePluginInfo {

    public static final String TAG_PLUGIN = "Trace";
    public static final String TAG_PLUGIN_FPS = TAG_PLUGIN + "_FPS";
    public static final String TAG_PLUGIN_EVIL_METHOD = TAG_PLUGIN + "_EvilMethod";
    public static final String TAG_PLUGIN_STARTUP = TAG_PLUGIN + "_StartUp";

//    public static final String ISSUE_DEVICE = "machine";
    public static final String ISSUE_SCENE = "scene";
    public static final String ISSUE_DROP_LEVEL = "dropLevel";
    public static final String ISSUE_DROP_SUM = "dropSum";
    public static final String ISSUE_FPS = "fps";
    public static final String ISSUE_STACK = "stack";
    public static final String ISSUE_UI_STACK = "uiStack";
    public static final String ISSUE_STACK_KEY = "stackKey";
    public static final String ISSUE_MEMORY = "memory";
    public static final String ISSUE_MEMORY_NATIVE = "native_heap";
    public static final String ISSUE_MEMORY_DALVIK = "dalvik_heap";
    public static final String ISSUE_MEMORY_VM_SIZE = "vm_size";
    public static final String ISSUE_COST = "cost";
    public static final String ISSUE_CPU_USAGE = "usage";
    public static final String ISSUE_STACK_TYPE = "detail";
    public static final String ISSUE_IS_WARM_START_UP = "is_warm_start_up";
    public static final String ISSUE_SUB_TYPE = "subType";
    public static final String STAGE_APPLICATION_CREATE = "application_create";
    public static final String STAGE_APPLICATION_CREATE_SCENE = "application_create_scene";
    public static final String STAGE_FIRST_ACTIVITY_CREATE = "first_activity_create";
    public static final String STAGE_STARTUP_DURATION = "startup_duration";
}
