package com.tencent.matrix.trace.tracer;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;
import android.os.SystemClock;

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

import java.util.LinkedList;
import java.util.List;

/**
 * Created by caichongyang on 2019/3/04.
 * <p>
 * firstMethod.i       LAUNCH_ACTIVITY   onWindowFocusChange   LAUNCH_ACTIVITY    onWindowFocusChange
 * ^                         ^                   ^                     ^                  ^
 * |                         |                   |                     |                  |
 * |---------app---------|---|---firstActivity---|---------...---------|---careActivity---|
 * |<--applicationCost-->|
 * |<----firstScreenCost---->|
 * |<---------------------------allCost(cold)------------------------->|
 * .                         |<--allCost(warm)-->|
 *
 * </p>
 */

public class StartupTracer extends Tracer implements IAppMethodBeatListener, Application.ActivityLifecycleCallbacks {

    private static final String TAG = "Matrix.StartupTracer";
    private final TraceConfig config;
    private long eggBrokenMs = 0;
    private long applicationCreateEnd = 0;
    private long applicationCost = 0;
    private long firstScreenCost = 0;
    private long coldCost = 0;
    private int activeActivityCount;


    public StartupTracer(TraceConfig config) {
        this.config = config;
    }

    @Override
    protected void onAlive() {
        if (config.isStartupEnable()) {
            AppMethodBeat.getInstance().addListener(this);
            Matrix.with().getApplication().registerActivityLifecycleCallbacks(this);
        }
    }

    @Override
    protected void onDead() {
        if (config.isStartupEnable()) {
            AppMethodBeat.getInstance().removeListener(this);
            Matrix.with().getApplication().unregisterActivityLifecycleCallbacks(this);
        }
    }


    @Override
    public void onApplicationCreated(long beginMs, long endMs, int scene) {
        this.eggBrokenMs = beginMs;
        this.applicationCreateEnd = endMs;
        this.applicationCost = endMs - beginMs;
    }

    @Override
    public void onActivityFocused(String activity) {
        long allCost = 0;
        boolean isWarmStartUp = false;
        if (isColdStartup()) {
            if (firstScreenCost == 0) {
                this.firstScreenCost = SystemClock.uptimeMillis() - eggBrokenMs;
            }
            if (config.getCareActivities().contains(activity)) {
                allCost = coldCost = SystemClock.uptimeMillis() - eggBrokenMs;
            } else if (config.getCareActivities().isEmpty()) {
                MatrixLog.i(TAG, "default care activity[%s]", activity);
                allCost = coldCost = firstScreenCost;
            } else {
                MatrixLog.w(TAG, "pass this activity[%s] in duration of startup!", activity);
            }
        } else if (isWarmStartUp = isWarmStartUp()) {
            allCost = SystemClock.uptimeMillis() - ActivityThreadHacker.sLastLaunchActivityTime;
        }

        if (allCost > 0) {
            analyse(applicationCost, firstScreenCost, allCost, isWarmStartUp);
        }
    }

    private boolean isColdStartup() {
        return coldCost == 0;
    }

    private boolean isWarmStartUp() {
        return activeActivityCount > 1 ? false : (SystemClock.uptimeMillis() - ActivityThreadHacker.sLastLaunchActivityTime > Constants.LIMIT_WARM_THRESHOLD_MS ? false : true);
    }

    private void analyse(long applicationCost, long firstScreenCost, long allCost, boolean isWarmStartUp) {
        MatrixLog.i(TAG, "[report] applicationCost:%s firstScreenCost:%s allCost:%s isWarmStartUp:%s", applicationCost, firstScreenCost, allCost, isWarmStartUp);
        long[] data = new long[0];
        if (!isWarmStartUp && allCost >= config.getColdStartupThresholdMs()) { // for cold startup
            data = AppMethodBeat.getInstance().copyData(ActivityThreadHacker.sApplicationCreateBeginMethodIndex);
            ActivityThreadHacker.sApplicationCreateBeginMethodIndex.release();

        } else if (isWarmStartUp && allCost >= config.getWarmStartupThresholdMs()) {
            data = AppMethodBeat.getInstance().copyData(ActivityThreadHacker.sLastLaunchActivityMethodIndex);
            ActivityThreadHacker.sLastLaunchActivityMethodIndex.release();
        }

        MatrixHandlerThread.getDefaultHandler().post(new AnalyseTask(data, applicationCost, firstScreenCost, allCost, isWarmStartUp));

    }

    private class AnalyseTask implements Runnable {

        long[] data;
        long applicationCost;
        long firstScreenCost;
        long allCost;
        boolean isWarmStartUp;

        AnalyseTask(long[] data, long applicationCost, long firstScreenCost, long allCost, boolean isWarmStartUp) {
            this.data = data;
            this.applicationCost = applicationCost;
            this.firstScreenCost = firstScreenCost;
            this.allCost = allCost;
            this.isWarmStartUp = isWarmStartUp;
        }

        @Override
        public void run() {
            LinkedList<MethodItem> stack = new LinkedList();
            TraceDataUtils.structuredDataToStack(data, stack, false);
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
                    List list = stack.subList(0, Constants.TARGET_EVIL_METHOD_STACK);
                    stack.clear();
                    stack.addAll(list);
                }
            });

            StringBuilder reportBuilder = new StringBuilder();
            StringBuilder logcatBuilder = new StringBuilder();
            long stackCost = Math.max(allCost, TraceDataUtils.stackToString(stack, reportBuilder, logcatBuilder));
            String stackKey = TraceDataUtils.getTreeKey(stack, Constants.MAX_LIMIT_ANALYSE_STACK_KEY_NUM);

            if ((allCost > config.getColdStartupThresholdMs() && !isWarmStartUp)
                    || (allCost > config.getWarmStartupThresholdMs() && isWarmStartUp)) {
                // logcat
                MatrixLog.w(TAG, "stackKey:%s \n%s", stackKey, logcatBuilder.toString());
            }

            // report
            report(applicationCost, firstScreenCost, reportBuilder, stackKey, stackCost, isWarmStartUp);
        }

        private void report(long applicationCost, long firstScreenCost, StringBuilder reportBuilder, String stackKey, long allCost, boolean isWarmStartUp) {
            TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
            try {
                JSONObject costObject = new JSONObject();
                costObject = DeviceUtil.getDeviceInfo(costObject, Matrix.with().getApplication());
                costObject.put(SharePluginInfo.STAGE_APPLICATION_CREATE, applicationCost);
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

            if ((allCost > config.getColdStartupThresholdMs() && !isWarmStartUp)
                    || (allCost > config.getWarmStartupThresholdMs() && isWarmStartUp)) {
                try {
                    JSONObject jsonObject = new JSONObject();
                    jsonObject = DeviceUtil.getDeviceInfo(jsonObject, Matrix.with().getApplication());
                    jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, Constants.Type.STARTUP);
                    jsonObject.put(SharePluginInfo.ISSUE_COST, allCost);
                    jsonObject.put(SharePluginInfo.ISSUE_STACK, reportBuilder.toString());
                    jsonObject.put(SharePluginInfo.ISSUE_STACK_KEY, stackKey);
                    jsonObject.put(SharePluginInfo.ISSUE_IS_WARM_START_UP, isWarmStartUp);
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


    @Override
    public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
        activeActivityCount++;
    }

    @Override
    public void onActivityDestroyed(Activity activity) {
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
}
