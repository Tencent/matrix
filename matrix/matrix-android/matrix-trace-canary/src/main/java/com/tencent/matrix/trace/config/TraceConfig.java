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
    public boolean defaultAppMethodBeatEnable = true;
    public boolean defaultAnrEnable;
    public boolean isDebug;
    public boolean isDevEnv;
    public String splashActivities;
    public Set<String> splashActivitiesSet;
    public boolean isHasActivity;

    private TraceConfig() {
        this.isHasActivity = true;
    }

    @Override
    public String toString() {
        StringBuilder ss = new StringBuilder(" \n");
        ss.append("# TraceConfig\n");
        ss.append("* isDebug:\t").append(isDebug).append("\n");
        ss.append("* isDevEnv:\t").append(isDevEnv).append("\n");
        ss.append("* isHasActivity:\t").append(isHasActivity).append("\n");
        ss.append("* defaultFpsEnable:\t").append(defaultFpsEnable).append("\n");
        ss.append("* defaultMethodTraceEnable:\t").append(defaultMethodTraceEnable).append("\n");
        ss.append("* defaultStartupEnable:\t").append(defaultStartupEnable).append("\n");
        ss.append("* defaultAnrEnable:\t").append(defaultAnrEnable).append("\n");
        ss.append("* splashActivities:\t").append(splashActivities).append("\n");
        return ss.toString();
    }

    @Override
    public boolean isAppMethodBeatEnable() {
        return defaultAppMethodBeatEnable;
    }

    @Override
    public boolean isFPSEnable() {
        return defaultFpsEnable;
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
        return defaultMethodTraceEnable;
    }

    public boolean isStartupEnable() {
        return defaultStartupEnable;
    }

    public boolean isHasActivity() {
        return isHasActivity;
    }

    @Override
    public boolean isAnrTraceEnable() {
        return defaultAnrEnable;
    }


    public Set<String> getSplashActivities() {
        if (null == splashActivitiesSet) {
            splashActivitiesSet = new HashSet<>();
            if (null == dynamicConfig) {
                if (null == splashActivities) {
                    return splashActivitiesSet;
                }
                splashActivitiesSet.addAll(Arrays.asList(splashActivities.split(";")));
            } else {

                String dySplashActivities = dynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_care_scene_set.name(), splashActivities);
                if (null != dySplashActivities) {
                    splashActivities = dySplashActivities;
                }

                if (null != splashActivities) {
                    splashActivitiesSet.addAll(Arrays.asList(splashActivities.split(";")));
                }
            }
        }
        return splashActivitiesSet;
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


    public int getFrozenThreshold() {
        return null == dynamicConfig
                ? Constants.DEFAULT_DROPPED_FROZEN
                : dynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_fps_dropped_frozen.name(), Constants.DEFAULT_DROPPED_FROZEN);
    }

    public int getHighThreshold() {
        return null == dynamicConfig
                ? Constants.DEFAULT_DROPPED_HIGH
                : dynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_fps_dropped_high.name(), Constants.DEFAULT_DROPPED_HIGH);
    }

    public int getMiddleThreshold() {
        return null == dynamicConfig
                ? Constants.DEFAULT_DROPPED_MIDDLE
                : dynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_fps_dropped_middle.name(), Constants.DEFAULT_DROPPED_MIDDLE);
    }

    public int getNormalThreshold() {
        return null == dynamicConfig
                ? Constants.DEFAULT_DROPPED_NORMAL
                : dynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_fps_dropped_normal.name(), Constants.DEFAULT_DROPPED_NORMAL);
    }


    public static class Builder {
        private TraceConfig config = new TraceConfig();

        public Builder dynamicConfig(IDynamicConfig dynamicConfig) {
            config.dynamicConfig = dynamicConfig;
            return this;
        }

        public Builder enableAppMethodBeat(boolean enable) {
            config.defaultAppMethodBeatEnable = enable;
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

        public Builder isDevEnv(boolean isDevEnv) {
            config.isDevEnv = isDevEnv;
            return this;
        }

        public Builder isHasActivity(boolean isHasActivity) {
            config.isHasActivity = isHasActivity;
            return this;
        }

        public Builder splashActivities(String activities) {
            config.splashActivities = activities;
            return this;
        }

        public TraceConfig build() {
            return config;
        }

    }


}
