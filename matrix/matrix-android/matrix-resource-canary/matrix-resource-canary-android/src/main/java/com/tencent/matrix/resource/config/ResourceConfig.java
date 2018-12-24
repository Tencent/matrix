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

package com.tencent.matrix.resource.config;

import com.tencent.mrs.plugin.IDynamicConfig;

import java.util.concurrent.TimeUnit;

/**
 * Created by tangyinsheng on 2017/6/2.
 */

public final class ResourceConfig {
    public static final String TAG = "Matrix.ResourceConfig";

    private static final long DEFAULT_DETECT_INTERVAL_MILLIS = TimeUnit.MINUTES.toMillis(1);
    private static final int DEFAULT_MAX_REDETECT_TIMES = 3;
    private static final boolean DEFAULT_DUMP_HPROF_ON = false;

    private final IDynamicConfig mDynamicConfig;
    private final boolean mDumpHprof;
    private final boolean mDetectDebugger;


    private ResourceConfig(IDynamicConfig dynamicConfig, boolean dumpHProf, boolean detectDebuger) {
        this.mDynamicConfig = dynamicConfig;
        this.mDumpHprof = dumpHProf;
        mDetectDebugger = detectDebuger;
    }

    public long getScanIntervalMillis() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_resource_detect_interval_millis.name(), DEFAULT_DETECT_INTERVAL_MILLIS);
    }

    public int getMaxRedetectTimes() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_resource_max_detect_times.name(), DEFAULT_MAX_REDETECT_TIMES);
    }

    public boolean getDumpHprof() {
        return mDumpHprof;
    }

    public boolean getDetectDebugger() {
        return mDetectDebugger;
    }

    public static final class Builder {

        private boolean mDumpHprof = DEFAULT_DUMP_HPROF_ON;
        private IDynamicConfig dynamicConfig;
        private boolean mDetectDebugger = false;

        public Builder dynamicConfig(IDynamicConfig dynamicConfig) {
            this.dynamicConfig = dynamicConfig;
            return this;
        }

        public Builder setDumpHprof(boolean enabled) {
            mDumpHprof = enabled;
            return this;
        }

        public Builder setDetectDebuger(boolean enabled) {
            mDetectDebugger = true;
            return this;
        }

        public ResourceConfig build() {
            return new ResourceConfig(dynamicConfig, mDumpHprof, mDetectDebugger);
        }
    }
}
