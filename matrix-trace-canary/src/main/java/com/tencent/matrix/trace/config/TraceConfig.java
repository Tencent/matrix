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

import android.app.Activity;
import android.support.annotation.NonNull;
import android.support.v4.app.Fragment;

import com.tencent.matrix.util.MatrixLog;
import com.tencent.mrs.plugin.IDynamicConfig;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.listeners.IDefaultConfig;
//import com.tencent.matrix.util.DeviceUtil;

import java.util.HashSet;

/**
 * Created by caichongyang on 17/5/18.
 * <p>
 * The config about TracePlugin setting.
 * </p>
 */

public class TraceConfig implements IDefaultConfig {
    private static final String TAG = "Matrix.TraceConfig";
    private final IDynamicConfig mDynamicConfig;
    private final boolean mDefaultFpsEnable;
    private final boolean mDefaultMethodTraceEnable;

    private TraceConfig(IDynamicConfig dynamicConfig, boolean enableFps, boolean methodTraceEnable) {
        this.mDynamicConfig = dynamicConfig;
        this.mDefaultFpsEnable = enableFps;
        this.mDefaultMethodTraceEnable = methodTraceEnable;
        MatrixLog.i(TAG, "enableFps:%b, methodTraceEnable:%b", enableFps, methodTraceEnable);
    }

    @Override
    public boolean isFPSEnable() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_fps_enable.name(), mDefaultFpsEnable);
    }

    @Override
    public boolean isMethodTraceEnable() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_evil_method_enable.name(), mDefaultMethodTraceEnable);
    }

    @Override
    public String toString() {
        return String.format("fpsEnable:%s,methodTraceEnable:%s,sceneSet:%s,fpsTimeSliceMs:%s,EvilThresholdNano:%sns,frameRefreshRate:%s",
                isFPSEnable(), isMethodTraceEnable(), getDynamicCareSceneSet(), getTimeSliceMs(), getEvilThresholdNano(), getFrameRreshRate());
    }

    @Override
    public boolean isTargetScene(String scene) {
        final String careSceneSet = getDynamicCareSceneSet();
        if ("".equals(careSceneSet)) { // "" means 'all'
            return true;
        }

        String[] vecScene = careSceneSet.split(";");
        HashSet<String> sceneSet = new HashSet<>();
        for (String tScene : vecScene) {
            if (!"".equals(tScene)) {
                sceneSet.add(tScene);
            }
        }

        return sceneSet.contains(scene);
    }

    private String getDynamicCareSceneSet() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_care_scene_set.name(), Constants.DEFAULT_CARE_SCENE_SET);
    }

    public static String getSceneForString(@NonNull Activity activity, Fragment fragment) {
        if (null == activity) {
            return "null";
        }
        return activity.getClass().getName() + (fragment == null ? "" : "&" + fragment.getClass().getName());
    }

    public long getEvilThresholdNano() {
//        String name = IDynamicConfig.ExptEnum.clicfg_matrix_trace_evil_method_threshold.name();
//        long real = mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_evil_method_threshold.name(), Constants.DEFAULT_EVIL_METHOD_THRESHOLD_MS) * Constants.TIME_MILLIS_TO_NANO;
//        MatrixLog.i(TAG, "name:%s , default:%d, real:%d", name, Constants.DEFAULT_EVIL_METHOD_THRESHOLD_MS, real / Constants.TIME_MILLIS_TO_NANO);
//        return real;
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_evil_method_threshold.name(), Constants.DEFAULT_EVIL_METHOD_THRESHOLD_MS) * Constants.TIME_MILLIS_TO_NANO;
    }

    public long getTimeSliceMs() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_fps_time_slice.name(), Constants.DEFAULT_FPS_TIME_SLICE_ALIVE_MS);
    }

//    public int getDeviceLevel(Context context) {
//        return 0 == mDeviceLevel ? mDeviceLevel = DeviceUtil.getLevel(context).getValue() : mDeviceLevel;
//    }

    public long getFPSReportInterval() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_fps_report_threshold.name(), Constants.DEFAULT_REPORT);
    }

    public long getLoadActivityThresholdMs() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_load_activity_threshold.name(), Constants.DEFAULT_ENTER_ACTIVITY_THRESHOLD_MS);
    }

    public long getStartUpThresholdMs() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_app_start_up_threshold.name(), Constants.DEFAULT_STARTUP_THRESHOLD_MS);
    }

    public long getWarmStartUpThresholdMs() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_app_start_up_threshold.name(), Constants.DEFAULT_STARTUP_THRESHOLD_MS_WARM);
    }

    public String getSplashActivityName() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_splash_activity_name.name(), Constants.DEFAULT_SPLASH_ACTIVITY_NAME);
    }

    public float getFrameRreshRate() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_app_start_up_threshold.name(), Constants.DEFAULT_DEVICE_REFRESH_RATE);
    }

    public boolean isHasSplashActivityName() {
        return !"".equals(mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_trace_splash_activity_name.name(), Constants.DEFAULT_SPLASH_ACTIVITY_NAME));
    }

    public static class Builder {
        private IDynamicConfig mDyConfig;
        private boolean mFPSEnable;
        private boolean mMethodTraceEnable;

        public Builder dynamicConfig(IDynamicConfig dynamicConfig) {
            this.mDyConfig = dynamicConfig;
            return this;
        }

        public Builder enableFPS(boolean enableFps) {
            this.mFPSEnable = enableFps;
            return this;
        }

        public Builder enableMethodTrace(boolean enableMethodTrace) {
            this.mMethodTraceEnable = enableMethodTrace;
            return this;
        }

        public TraceConfig build() {
            return new TraceConfig(mDyConfig, mFPSEnable, mMethodTraceEnable);
        }

    }


}
