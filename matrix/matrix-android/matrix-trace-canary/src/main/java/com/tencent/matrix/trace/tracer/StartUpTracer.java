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

package com.tencent.matrix.trace.tracer;

import android.app.Activity;
import android.os.Handler;
import android.os.SystemClock;
import android.text.TextUtils;

import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.SharePluginInfo;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.core.OldMethodBeat;
import com.tencent.matrix.trace.hacker.ActivityThreadHacker;
import com.tencent.matrix.trace.listeners.IMethodBeatListener;
import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;

/**
 * Created by caichongyang on 2017/5/26.
 *
 * |----app----|--between--|--firstAc---|---secondAc----|
 */

public class StartUpTracer extends BaseTracer implements IMethodBeatListener {
    private static final String TAG = "Matrix.StartUpTracer";
    private final TraceConfig mTraceConfig;
    private boolean isFirstActivityCreate = true;
    private String mFirstActivityName = null;
    private static int mFirstActivityIndex;
    private final HashMap<String, Long> mFirstActivityMap = new HashMap<>();
    private final HashMap<String, Long> mActivityEnteredMap = new HashMap<>();
    private final Handler mHandler;

    public StartUpTracer(TracePlugin plugin, TraceConfig config) {
        super(plugin);
        this.mTraceConfig = config;
        this.mHandler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper());
    }

    @Override
    protected boolean isEnableMethodBeat() {
        return true;
    }

    @Override
    protected String getTag() {
        return SharePluginInfo.TAG_PLUGIN_STARTUP;
    }

    @Override
    public void onActivityCreated(Activity activity) {
        super.onActivityCreated(activity);
        if (isFirstActivityCreate && mFirstActivityMap.isEmpty()) {
            String activityName = activity.getComponentName().getClassName();
            mFirstActivityIndex = getMethodBeat().getCurIndex();
            mFirstActivityName = activityName;
            mFirstActivityMap.put(activityName, SystemClock.uptimeMillis());
            MatrixLog.i(TAG, "[onActivityCreated] first activity:%s index:%s local time:%s", mFirstActivityName, mFirstActivityIndex, System.currentTimeMillis());
            getMethodBeat().lockBuffer(true);
        }
    }

    @Override
    public void onBackground(Activity activity) {
        super.onBackground(activity);
        isFirstActivityCreate = true;
    }

    @Override
    public void onActivityEntered(Activity activity, boolean isFocus, int nowIndex, long[] buffer) {
        if (mFirstActivityName == null) {
            isFirstActivityCreate = false;
            getMethodBeat().lockBuffer(false);
            return;
        }
        String activityName = activity.getComponentName().getClassName();
        if (!mActivityEnteredMap.containsKey(activityName) || isFocus) {
            mActivityEnteredMap.put(activityName, SystemClock.uptimeMillis());
        }
        if (!isFocus) {
            MatrixLog.i(TAG, "[onActivityEntered] isFocus false,activityName:%s", activityName);
            return;
        }

        if (mTraceConfig.isHasSplashActivityName() && activityName.equals(mTraceConfig.getSplashActivityName())) {
            MatrixLog.i(TAG, "[onActivityEntered] has splash activity! %s", mTraceConfig.getSplashActivityName());
            return;
        }

        getMethodBeat().lockBuffer(false);

        long activityEndTime = getValueFromMap(mActivityEnteredMap, activityName);
        long firstActivityStartTime = getValueFromMap(mFirstActivityMap, mFirstActivityName);
        if (activityEndTime <= 0 || firstActivityStartTime <= 0) {
            MatrixLog.w(TAG, "[onActivityEntered] error activityCost! [%s:%s]", activityEndTime, firstActivityStartTime);
            mFirstActivityMap.clear();
            mActivityEnteredMap.clear();
            return;
        }

        boolean isWarnStartUp = isWarmStartUp(firstActivityStartTime);
        long activityCost = activityEndTime - firstActivityStartTime;
        long appCreateTime = ActivityThreadHacker.sApplicationCreateEndTime - ActivityThreadHacker.sApplicationCreateBeginTime;
        long betweenCost = firstActivityStartTime - ActivityThreadHacker.sApplicationCreateEndTime;
        long allCost = activityEndTime - ActivityThreadHacker.sApplicationCreateBeginTime;

        if (isWarnStartUp) {
            betweenCost = 0;
            allCost = activityCost;
        }
        long splashCost = 0;
        if (mTraceConfig.isHasSplashActivityName()) {
            long tmp = getValueFromMap(mActivityEnteredMap, mTraceConfig.getSplashActivityName());
            splashCost = tmp == 0 ? 0 : getValueFromMap(mActivityEnteredMap, activityName) - tmp;
        }
        if (appCreateTime <= 0 || (mTraceConfig.isHasSplashActivityName() && splashCost < 0)) {
            MatrixLog.e(TAG, "[onActivityEntered] is wrong! appCreateTime:%s isHasSplashActivityName:%s splashCost:%s", appCreateTime, mTraceConfig.isHasSplashActivityName(), splashCost);
            mFirstActivityMap.clear();
            mActivityEnteredMap.clear();
            return;
        }

        EvilMethodTracer tracer = getTracer(EvilMethodTracer.class);
        if (null != tracer) {
            long thresholdMs = isWarnStartUp ? mTraceConfig.getWarmStartUpThresholdMs() : mTraceConfig.getStartUpThresholdMs();
            int startIndex = isWarnStartUp ? mFirstActivityIndex : ActivityThreadHacker.sApplicationCreateBeginMethodIndex;
            int curIndex = getMethodBeat().getCurIndex();
            if (allCost > thresholdMs) {
                MatrixLog.i(TAG, "appCreateTime[%s] is over threshold![%s], dump stack! index[%s:%s]", appCreateTime, thresholdMs, startIndex, curIndex);
                EvilMethodTracer evilMethodTracer = getTracer(EvilMethodTracer.class);
                if (null != evilMethodTracer) {
                    evilMethodTracer.handleBuffer(EvilMethodTracer.Type.STARTUP, startIndex, curIndex, OldMethodBeat.getBuffer(), appCreateTime, Constants.SUBTYPE_STARTUP_APPLICATION);
                }
            }
        }

        MatrixLog.i(TAG, "[onActivityEntered] firstActivity:%s appCreateTime:%dms betweenCost:%dms activityCreate:%dms splashCost:%dms allCost:%sms isWarnStartUp:%b ApplicationCreateScene:%s",
                mFirstActivityName, appCreateTime, betweenCost, activityCost, splashCost, allCost, isWarnStartUp, ActivityThreadHacker.sApplicationCreateScene);

        mHandler.post(new StartUpReportTask(activityName, appCreateTime, activityCost, betweenCost, splashCost, allCost, isWarnStartUp, ActivityThreadHacker.sApplicationCreateScene));

        mFirstActivityMap.clear();
        mActivityEnteredMap.clear();
        isFirstActivityCreate = false;
        mFirstActivityName = null;
        onDestroy();

    }

    @Override
    public void onCreate() {
        if (!isHasDestroy) {
            super.onCreate();
        }
    }

    private static boolean isHasDestroy = false;

    @Override
    public void onDestroy() {
        super.onDestroy();
        isHasDestroy = true;
    }

    long getValueFromMap(HashMap<String, Long> map, String key) {
        if (null == map || key == null || !map.containsKey(key)) {
            MatrixLog.w(TAG, "[getValueFromMap] key:%s", TextUtils.isEmpty(key) ? "null" : key);
            return 0;
        }
        return map.get(key);
    }

    @Override
    public void onApplicationCreated(long startTime, long endTime) {
        long cost = endTime - startTime;
        MatrixLog.i(TAG, "[onApplicationCreated] application create cost:%dms startTime:%s endTime:%s", cost, startTime, endTime);
    }

    private boolean isWarmStartUp(long time) {
        return time - ActivityThreadHacker.sApplicationCreateEndTime > Constants.LIMIT_WARM_THRESHOLD_MS;
    }

    private class StartUpReportTask implements Runnable {
        long appCost;
        long activityCost;
        long betweenCost;
        long splashCost;
        long allCost;
        int scene;
        String activityName;
        boolean isWarmStartUp;

        StartUpReportTask(String activityName, long appCost, long activityCost, long betweenCost, long splashCost, long allCost, boolean isWarmStartUp, int scene) {
            this.appCost = appCost;
            this.activityCost = activityCost;
            this.betweenCost = betweenCost;
            this.splashCost = splashCost;
            this.allCost = allCost;
            this.activityName = activityName;
            this.isWarmStartUp = isWarmStartUp;
            this.scene = scene;
        }

        @Override
        public void run() {
            JSONObject jsonObject = new JSONObject();
            try {
                jsonObject = DeviceUtil.getDeviceInfo(jsonObject, getPlugin().getApplication());

                jsonObject.put(SharePluginInfo.STAGE_APPLICATION_CREATE, appCost);
                jsonObject.put(SharePluginInfo.STAGE_FIRST_ACTIVITY_CREATE, activityCost);
                jsonObject.put(SharePluginInfo.STAGE_BETWEEN_APP_AND_ACTIVITY, betweenCost);
                jsonObject.put(SharePluginInfo.STAGE_SPLASH_ACTIVITY_DURATION, splashCost);
                jsonObject.put(SharePluginInfo.STAGE_STARTUP_DURATION, allCost);
                jsonObject.put(SharePluginInfo.ISSUE_SCENE, activityName);
                jsonObject.put(SharePluginInfo.ISSUE_IS_WARM_START_UP, isWarmStartUp);
                jsonObject.put(SharePluginInfo.STAGE_APPLICATION_CREATE_SCENE, scene);
                sendReport(jsonObject, SharePluginInfo.TAG_PLUGIN_STARTUP);
            } catch (JSONException e) {
                MatrixLog.e(TAG, "[JSONException for StartUpReportTask error: %s", e);
            }
        }
    }
}
