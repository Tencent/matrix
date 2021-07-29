/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
import android.app.Application;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.SharePluginInfo;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.core.AppMethodBeat;
import com.tencent.matrix.trace.hacker.ActivityThreadHacker;
import com.tencent.matrix.trace.items.MethodItem;
import com.tencent.matrix.trace.listeners.IAppMethodBeatListener;
import com.tencent.matrix.trace.util.TraceDataUtils;
import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import org.json.JSONException;
import org.json.JSONObject;

import java.lang.reflect.Field;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

import static android.os.SystemClock.uptimeMillis;

/**
 * Created by caichongyang on 2019/3/04.
 * <p>
 * firstMethod.i       LAUNCH_ACTIVITY   onWindowFocusChange   LAUNCH_ACTIVITY    onWindowFocusChange
 * ^                         ^                   ^                     ^                  ^
 * |                         |                   |                     |                  |
 * |---------app---------|---|---firstActivity---|---------...---------|---careActivity---|
 * |<--applicationCost-->|
 * |<--------------firstScreenCost-------------->|
 * |<---------------------------------------coldCost------------------------------------->|
 * .                         |<-----warmCost---->|
 *
 * </p>
 */

public class StartupTracer extends Tracer implements IAppMethodBeatListener, ActivityThreadHacker.IApplicationCreateListener, Application.ActivityLifecycleCallbacks {

    private static final String TAG = "Matrix.StartupTracer";
    private final TraceConfig config;
    private long firstScreenCost = 0;
    private long coldCost = 0;
    private int activeActivityCount;
    private boolean isWarmStartUp;
    private boolean hasShowSplashActivity;
    private boolean isStartupEnable;
    private Set<String> splashActivities;
    private long coldStartupThresholdMs;
    private long warmStartupThresholdMs;
    private boolean isHasActivity;


    public StartupTracer(TraceConfig config) {
        this.config = config;
        this.isStartupEnable = config.isStartupEnable();
        this.splashActivities = config.getSplashActivities();
        this.coldStartupThresholdMs = config.getColdStartupThresholdMs();
        this.warmStartupThresholdMs = config.getWarmStartupThresholdMs();
        this.isHasActivity = config.isHasActivity();
        ActivityThreadHacker.addListener(this);
    }

    @Override
    protected void onAlive() {
        super.onAlive();
        MatrixLog.i(TAG, "[onAlive] isStartupEnable:%s", isStartupEnable);
        if (isStartupEnable) {
            AppMethodBeat.getInstance().addListener(this);
            Matrix.with().getApplication().registerActivityLifecycleCallbacks(this);
        }
    }

    @Override
    protected void onDead() {
        super.onDead();
        if (isStartupEnable) {
            AppMethodBeat.getInstance().removeListener(this);
            Matrix.with().getApplication().unregisterActivityLifecycleCallbacks(this);
        }
    }

    @Override
    public void onApplicationCreateEnd() {
        if (!isHasActivity) {
            long applicationCost = ActivityThreadHacker.getApplicationCost();
            MatrixLog.i(TAG, "onApplicationCreateEnd, applicationCost:%d", applicationCost);
            analyse(applicationCost, 0, applicationCost, false);
        }
    }

    @Override
    public void onActivityFocused(Activity activity) {
        if (ActivityThreadHacker.sApplicationCreateScene == Integer.MIN_VALUE) {
            Log.w(TAG, "start up from unknown scene");
            return;
        }

        String activityName = activity.getClass().getName();
        if (isColdStartup()) {
            boolean isCreatedByLaunchActivity = ActivityThreadHacker.isCreatedByLaunchActivity();
            MatrixLog.i(TAG, "#ColdStartup# activity:%s, splashActivities:%s, empty:%b, "
                            + "isCreatedByLaunchActivity:%b, hasShowSplashActivity:%b, "
                            + "firstScreenCost:%d, now:%d, application_create_begin_time:%d, app_cost:%d",
                    activityName, splashActivities, splashActivities.isEmpty(), isCreatedByLaunchActivity,
                    hasShowSplashActivity, firstScreenCost, uptimeMillis(),
                    ActivityThreadHacker.getEggBrokenTime(), ActivityThreadHacker.getApplicationCost());

            String key = activityName + "@" + activity.hashCode();
            Long createdTime = createdTimeMap.get(key);
            if (createdTime == null) {
                createdTime = 0L;
            }
            createdTimeMap.put(key, uptimeMillis() - createdTime);

            if (firstScreenCost == 0) {
                this.firstScreenCost = uptimeMillis() - ActivityThreadHacker.getEggBrokenTime();
            }
            if (hasShowSplashActivity) {
                coldCost = uptimeMillis() - ActivityThreadHacker.getEggBrokenTime();
            } else {
                if (splashActivities.contains(activityName)) {
                    hasShowSplashActivity = true;
                } else if (splashActivities.isEmpty()) { //process which is has activity but not main UI process
                    if (isCreatedByLaunchActivity) {
                        coldCost = firstScreenCost;
                    } else {
                        firstScreenCost = 0;
                        coldCost = ActivityThreadHacker.getApplicationCost();
                    }
                } else {
                    if (isCreatedByLaunchActivity) {
//                        MatrixLog.e(TAG, "pass this activity[%s] at duration of start up! splashActivities=%s", activity, splashActivities);
                        coldCost = firstScreenCost;
                    } else {
                        firstScreenCost = 0;
                        coldCost = ActivityThreadHacker.getApplicationCost();
                    }
                }
            }
            if (coldCost > 0) {
                Long betweenCost = createdTimeMap.get(key);
                if (null != betweenCost && betweenCost >= 30 * 1000) {
                    MatrixLog.e(TAG, "%s cost too much time[%s] between activity create and onActivityFocused, "
                            + "just throw it.(createTime:%s) ", key, uptimeMillis() - createdTime, createdTime);
                    return;
                }
                analyse(ActivityThreadHacker.getApplicationCost(), firstScreenCost, coldCost, false);
            }

        } else if (isWarmStartUp()) {
            isWarmStartUp = false;
            long warmCost = uptimeMillis() - lastCreateActivity;
            MatrixLog.i(TAG, "#WarmStartup# activity:%s, warmCost:%d, now:%d, lastCreateActivity:%d", activityName, warmCost, uptimeMillis(), lastCreateActivity);

            if (warmCost > 0) {
                analyse(0, 0, warmCost, true);
            }
        }

    }

    private boolean isColdStartup() {
        return coldCost == 0;
    }

    private boolean isWarmStartUp() {
        return isWarmStartUp;
    }

    private void analyse(long applicationCost, long firstScreenCost, long allCost, boolean isWarmStartUp) {
        MatrixLog.i(TAG, "[report] applicationCost:%s firstScreenCost:%s allCost:%s isWarmStartUp:%s, createScene:%d",
                applicationCost, firstScreenCost, allCost, isWarmStartUp, ActivityThreadHacker.sApplicationCreateScene);
        long[] data = new long[0];
        if (!isWarmStartUp && allCost >= coldStartupThresholdMs) { // for cold startup
            data = AppMethodBeat.getInstance().copyData(ActivityThreadHacker.sApplicationCreateBeginMethodIndex);
            ActivityThreadHacker.sApplicationCreateBeginMethodIndex.release();

        } else if (isWarmStartUp && allCost >= warmStartupThresholdMs) {
            data = AppMethodBeat.getInstance().copyData(ActivityThreadHacker.sLastLaunchActivityMethodIndex);
            ActivityThreadHacker.sLastLaunchActivityMethodIndex.release();
        }

        MatrixHandlerThread.getDefaultHandler().post(new AnalyseTask(data, applicationCost, firstScreenCost, allCost, isWarmStartUp, ActivityThreadHacker.sApplicationCreateScene));

    }

    private class AnalyseTask implements Runnable {

        long[] data;
        long applicationCost;
        long firstScreenCost;
        long allCost;
        boolean isWarmStartUp;
        int scene;

        AnalyseTask(long[] data, long applicationCost, long firstScreenCost, long allCost, boolean isWarmStartUp, int scene) {
            this.data = data;
            this.scene = scene;
            this.applicationCost = applicationCost;
            this.firstScreenCost = firstScreenCost;
            this.allCost = allCost;
            this.isWarmStartUp = isWarmStartUp;
        }

        @Override
        public void run() {
            LinkedList<MethodItem> stack = new LinkedList();
            if (data.length > 0) {
                TraceDataUtils.structuredDataToStack(data, stack, false, -1);
                TraceDataUtils.trimStack(stack, Constants.TARGET_EVIL_METHOD_STACK, new TraceDataUtils.IStructuredDataFilter() {
                    @Override
                    public boolean isFilter(long during, int filterCount) {
                        return during < filterCount * Constants.TIME_UPDATE_CYCLE_MS;
                    }

                    @Override
                    public int getFilterMaxCount() {
                        return Constants.FILTER_STACK_MAX_COUNT;
                    }

                    @Override
                    public void fallback(List<MethodItem> stack, int size) {
                        MatrixLog.w(TAG, "[fallback] size:%s targetSize:%s stack:%s", size, Constants.TARGET_EVIL_METHOD_STACK, stack);
                        Iterator iterator = stack.listIterator(Math.min(size, Constants.TARGET_EVIL_METHOD_STACK));
                        while (iterator.hasNext()) {
                            iterator.next();
                            iterator.remove();
                        }

                    }
                });
            }

            StringBuilder reportBuilder = new StringBuilder();
            StringBuilder logcatBuilder = new StringBuilder();
            long stackCost = Math.max(allCost, TraceDataUtils.stackToString(stack, reportBuilder, logcatBuilder));
            String stackKey = TraceDataUtils.getTreeKey(stack, stackCost);

            // for logcat
            if ((allCost > coldStartupThresholdMs && !isWarmStartUp)
                    || (allCost > warmStartupThresholdMs && isWarmStartUp)) {
                MatrixLog.w(TAG, "stackKey:%s \n%s", stackKey, logcatBuilder.toString());
            }

            // report
            report(applicationCost, firstScreenCost, reportBuilder, stackKey, stackCost, isWarmStartUp, scene);
        }

        private void report(long applicationCost, long firstScreenCost, StringBuilder reportBuilder, String stackKey,
                            long allCost, boolean isWarmStartUp, int scene) {

            TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
            if (null == plugin) {
                return;
            }
            try {
                JSONObject costObject = new JSONObject();
                costObject = DeviceUtil.getDeviceInfo(costObject, Matrix.with().getApplication());
                costObject.put(SharePluginInfo.STAGE_APPLICATION_CREATE, applicationCost);
                costObject.put(SharePluginInfo.STAGE_APPLICATION_CREATE_SCENE, scene);
                costObject.put(SharePluginInfo.STAGE_FIRST_ACTIVITY_CREATE, firstScreenCost);
                costObject.put(SharePluginInfo.STAGE_STARTUP_DURATION, allCost);
                costObject.put(SharePluginInfo.ISSUE_IS_WARM_START_UP, isWarmStartUp);
                Issue issue = new Issue();
                issue.setTag(SharePluginInfo.TAG_PLUGIN_STARTUP);
                issue.setContent(costObject);
                plugin.onDetectIssue(issue);
            } catch (JSONException e) {
                MatrixLog.e(TAG, "[JSONException for StartUpReportTask error: %s", e);
            }


            if ((allCost > coldStartupThresholdMs && !isWarmStartUp)
                    || (allCost > warmStartupThresholdMs && isWarmStartUp)) {

                try {
                    JSONObject jsonObject = new JSONObject();
                    jsonObject = DeviceUtil.getDeviceInfo(jsonObject, Matrix.with().getApplication());
                    jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, Constants.Type.STARTUP);
                    jsonObject.put(SharePluginInfo.ISSUE_COST, allCost);
                    jsonObject.put(SharePluginInfo.ISSUE_TRACE_STACK, reportBuilder.toString());
                    jsonObject.put(SharePluginInfo.ISSUE_STACK_KEY, stackKey);
                    jsonObject.put(SharePluginInfo.ISSUE_SUB_TYPE, isWarmStartUp ? 2 : 1);
                    Issue issue = new Issue();
                    issue.setTag(SharePluginInfo.TAG_PLUGIN_EVIL_METHOD);
                    issue.setContent(jsonObject);
                    plugin.onDetectIssue(issue);

                } catch (JSONException e) {
                    MatrixLog.e(TAG, "[JSONException error: %s", e);
                }
            }
        }
    }

    private long lastCreateActivity = 0L;
    private HashMap<String, Long> createdTimeMap = new HashMap<>();
    private boolean isShouldRecordCreateTime = true;

    @Override
    public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
        MatrixLog.i(TAG, "activeActivityCount:%d, coldCost:%d", activeActivityCount, coldCost);
        if (activeActivityCount == 0 && coldCost > 0) {
            lastCreateActivity = uptimeMillis();
            MatrixLog.i(TAG, "lastCreateActivity:%d, activity:%s", lastCreateActivity, activity.getClass().getName());
            isWarmStartUp = true;
        }
        activeActivityCount++;
        if (isShouldRecordCreateTime) {
            createdTimeMap.put(activity.getClass().getName() + "@" + activity.hashCode(), uptimeMillis());
        }
    }

    @Override
    public void onActivityDestroyed(Activity activity) {
        MatrixLog.i(TAG, "activeActivityCount:%d", activeActivityCount);
        activeActivityCount--;
    }

    @Override
    public void onActivityStarted(Activity activity) {

    }

    @Override
    public void onActivityResumed(Activity activity) {

    }

    @Override
    public void onActivityPaused(Activity activity) {

    }

    @Override
    public void onActivityStopped(Activity activity) {

    }

    @Override
    public void onActivitySaveInstanceState(Activity activity, Bundle outState) {

    }

    @Override
    public void onForeground(boolean isForeground) {
        super.onForeground(isForeground);
        if (!isForeground) {
            checkActivityThread_mCallback();
        }
    }

    private static void checkActivityThread_mCallback() {
        try {
            Class<?> forName = Class.forName("android.app.ActivityThread");
            Field field = forName.getDeclaredField("sCurrentActivityThread");
            field.setAccessible(true);
            Object activityThreadValue = field.get(forName);
            Field mH = forName.getDeclaredField("mH");
            mH.setAccessible(true);
            Object handler = mH.get(activityThreadValue);
            Class<?> handlerClass = handler.getClass().getSuperclass();
            Field callbackField = handlerClass.getDeclaredField("mCallback");
            callbackField.setAccessible(true);
            Handler.Callback currentCallback = (Handler.Callback) callbackField.get(handler);
            MatrixLog.i(TAG, "callback %s", currentCallback);

        } catch (Exception e) {
        }
    }

}
