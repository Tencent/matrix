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

import com.tencent.matrix.util.MatrixLog;
import com.tencent.mrs.plugin.IDynamicConfig;

import java.util.HashSet;

/**
 * @author zhouzhijie
 * Created by zhouzhijie on 2018/9/12.
 */

public final class MemoryConfig {
    public static final String TAG = "Matrix.MemoryConfig";

    /**
     * The default, lax policy will enable all available detectors
     */

//    private static final int mMaxApiCost = 1000; // not used
    private static final int DEFAULT_MIDDLE_MIN_SPAN = 500;
    private static final int DEFAULT_HIGH_MIN_SPAN = 500;
    private static final float DEFAULT_THRESHOLD = 0.9f;
    private static final String DEFAULT_SPECIAL_ACTIVITIES = "";

    private final IDynamicConfig mDynamicConfig;

    private   MemoryConfig(IDynamicConfig dynamicConfig) {
        this.mDynamicConfig = dynamicConfig;
        MatrixLog.i(TAG, "MemoryConfig()");
    }

    public int getMiddleMinSpan() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_memory_middle_min_span.name(), DEFAULT_MIDDLE_MIN_SPAN);
    }

    public int getHighMinSpan() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_memory_high_min_span.name(), DEFAULT_HIGH_MIN_SPAN);
    }

    public float getThreshold() {
//        String name = IDynamicConfig.ExptEnum.clicfg_matrix_memory_threshold.name();
//        float real = mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_memory_threshold.name(), DEFAULT_THRESHOLD);
//        MatrixLog.i(TAG, "name:%s, default:%f, real:%f", name, DEFAULT_THRESHOLD, real);
//        return  real;
          return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_memory_threshold.name(), DEFAULT_THRESHOLD);
    }

    public static int getDefaultHighMinSpan() {
        return DEFAULT_HIGH_MIN_SPAN;
    }

    public HashSet<String> getSpecialActivities() {
        String strActivities = mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_memory_special_activities.name(), DEFAULT_SPECIAL_ACTIVITIES);
        if (strActivities == null || "".equals(strActivities)) {
            return null;
        }

        String[] vecActivities = strActivities.split(";");
        HashSet<String> result = new HashSet<>();
        for (String activity : vecActivities) {
            if ("".equals(activity)) {
                continue;
            }

            result.add(activity);
        }
        return result;
    }

//
    public static final class Builder {

        private IDynamicConfig dynamicConfig;

        public Builder dynamicConfig(IDynamicConfig dynamicConfig) {
            this.dynamicConfig = dynamicConfig;
            return this;
        }

        public  MemoryConfig build() {
            return new MemoryConfig(dynamicConfig);
        }
    }
}
