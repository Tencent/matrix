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

import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.listeners.IDefaultConfig;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.mrs.plugin.IDynamicConfig;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;


/**
 * Created by caichongyang on 17/5/18.
 * <p>
 * The config about TracePlugin setting.
 * </p>
 */

public class TraceConfig implements IDefaultConfig {
    private static final String TAG = "Matrix.TraceConfig";
    public IDynamicConfig dynamicConfig;
    public boolean defaultFpsEnable;
    public boolean defaultMethodTraceEnable;
    public boolean defaultStartupEnable;
    public boolean defaultAnrEnable;
    public boolean isDebug;
    public boolean isDevEnv = true;
    public String careActivities;
    public Set<String> careActivitiesSet;


    private TraceConfig() {

    }

    @Override
    public String toString() {
        StringBuilder ss = new StringBuilder(" \n");
        ss.append("# TraceConfig\n");
        ss.append("* isDebug:\t").append(isDebug).append("\n");
        ss.append("* isDevEnv:\t").append(isDevEnv).append("\n");
        ss.append("* defaultFpsEnable:\t").append(defaultFpsEnable).append("\n");
        ss.append("* defaultMethodTraceEnable:\t").append(defaultMethodTraceEnable).append("\n");
        ss.append("* defaultStartupEnable:\t").append(defaultStartupEnable).append("\n");
        ss.append("* defaultAnrEnable:\t").append(defaultAnrEnable).append("\n");
        ss.append("* careActivities:\t").append(careActivities).append("\n");
        return ss.toString();
    }

    @Override
    public boolean isFPSEnable() {
        return dynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_fps_enable.name(), defaultFpsEnable);
    }

    @Override
    public boolean isDebug() {
        return isDebug;
    }

    @Override

    public boolean isDevEnv() {
        return isDevEnv;
    }

    @Override
    public boolean isEvilMethodTraceEnable() {
        return null == dynamicConfig
                ? defaultMethodTraceEnable
                : dynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_evil_method_enable.name(), defaultMethodTraceEnable);
    }

    public boolean isStartupEnable() {
        return dynamicConfig == null
                ? defaultStartupEnable
                : dynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_startup_enable.name(), defaultStartupEnable);
    }

    @Override
    public boolean isAnrTraceEnable() {
        return dynamicConfig == null
                ? defaultAnrEnable
                : dynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_anr_enable.name(), defaultAnrEnable);
    }


    public Set<String> getCareActivities() {
        if (null == careActivitiesSet) {
            careActivitiesSet = (null == dynamicConfig
                    ? new HashSet<>(Arrays.asList(careActivities))
                    : new HashSet<>(Arrays.asList(dynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_care_scene_set.name(), careActivities).split(";"))));
        }
        return careActivitiesSet;
    }


    public int getEvilThresholdMs() {
        return null == dynamicConfig
                ? Constants.DEFAULT_EVIL_METHOD_THRESHOLD_MS
                : dynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_evil_method_threshold.name(), Constants.DEFAULT_EVIL_METHOD_THRESHOLD_MS);
    }

    public int getTimeSliceMs() {
        return null == dynamicConfig
                ? Constants.DEFAULT_FPS_TIME_SLICE_ALIVE_MS
                : dynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_fps_time_slice.name(), Constants.DEFAULT_FPS_TIME_SLICE_ALIVE_MS);
    }


    public int getColdStartupThresholdMs() {
        return null == dynamicConfig
                ? Constants.DEFAULT_STARTUP_THRESHOLD_MS_COLD
                : dynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_app_start_up_threshold.name(), Constants.DEFAULT_STARTUP_THRESHOLD_MS_COLD);
    }

    public int getWarmStartupThresholdMs() {
        return null == dynamicConfig
                ? Constants.DEFAULT_STARTUP_THRESHOLD_MS_WARM
                : dynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_warm_app_start_up_threshold.name(), Constants.DEFAULT_STARTUP_THRESHOLD_MS_WARM);
    }


    public static class Builder {
        private TraceConfig config = new TraceConfig();

        public Builder dynamicConfig(IDynamicConfig dynamicConfig) {
            config.dynamicConfig = dynamicConfig;
            return this;
        }

        public Builder enableFPS(boolean enable) {
            config.defaultFpsEnable = enable;
            return this;
        }

        public Builder enableEvilMethodTrace(boolean enable) {
            config.defaultMethodTraceEnable = enable;
            return this;
        }

        public Builder enableAnrTrace(boolean enable) {
            config.defaultAnrEnable = enable;
            return this;
        }

        public Builder enableStartup(boolean enable) {
            config.defaultStartupEnable = enable;
            return this;
        }

        public Builder isDebug(boolean isDebug) {
            config.isDebug = isDebug;
            return this;
        }

        public Builder careActivities(String activities) {
            config.careActivities = activities;
            return this;
        }

        public TraceConfig build() {
            MatrixLog.i(TAG, config.toString());
            return config;
        }

    }


}
