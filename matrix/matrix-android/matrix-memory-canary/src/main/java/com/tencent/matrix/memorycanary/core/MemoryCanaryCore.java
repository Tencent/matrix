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

package com.tencent.matrix.memorycanary.core;

import com.tencent.matrix.memorycanary.MemoryCanaryPlugin;
import com.tencent.matrix.memorycanary.config.MemoryConfig;
import com.tencent.matrix.memorycanary.config.SharePluginInfo;
import com.tencent.matrix.memorycanary.util.DebugMemoryInfoUtil;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.report.IssuePublisher;
import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.plugin.PluginShareConstants;

import android.app.Activity;
import android.app.Application;
import android.content.ComponentCallbacks2;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.os.Debug;
import android.os.Handler;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;

/**
 * Created by astrozhou on 2018/9/18.
 */
public class MemoryCanaryCore implements IssuePublisher.OnIssueDetectListener {

    public static class MatrixMemoryInfo {
        MatrixMemoryInfo(final String activity) {
            mDalvikHeap = mNativeHeap = mJavaHeap = mNativePss = mGraphics = mStack = mCode = mOther = mTotalPss = mTotalUss = -1;
            mActivity = activity;
        }

        public int mDalvikHeap;
        public int mNativeHeap;
        public int mJavaHeap;
        public int mNativePss;
        public int mGraphics;
        public int mStack;
        public int mCode;
        public int mOther;
        public int mTotalPss;
        public int mTotalUss;
        public String mActivity;
    }

    private static final String TAG = "Matrix.MemoryCanaryCore";
    private static final String JAVA_HEAP = "summary.java-heap";
    private static final String NATIVE_HEAP = "summary.native-heap";
    private static final String CODE_MEM = "summary.code";
    private static final String STACK_MEM = "summary.stack";
    private static final String GRAPHICS_MEM = "summary.graphics";
    private static final String PRIVATE_OTHER = "summary.private-other";
    private static final String TOTAL_PSS = "summary.total-pss";
    private static final int LOW_JAVA_HEAP_FLAG = 1;
    private static final int LOW_NATIVE_HEAP_FLAG = 2;
    private static final int LOW_MEMORY_TRIM = 3;
    private static final int LOW_VMSIZE_FLAG = 4;

    private static final int STEP_FACTOR = 60 * 1000;
    private static final int MAX_STEP = 30 * STEP_FACTOR;
    private static final int NATIVE_HEAP_LIMIT = 500 * 1024;
    private static final int TRIM_MEMORY_SPAN = 10 * 60 * 1000;
    private static final int VMSIZE_LIMIT = 4 * 1024 * 1024; //4GB
    private static final int MAX_COST = 3000;

    private final MemoryCanaryPlugin mPlugin;
    private boolean mIsOpen = false;
    private DeviceUtil.LEVEL mLevel;
    private static long mTotalMemory = 0;
    private static long mLowMemoryThreshold = 0;
    private static int mMemoryClass = 0;
    private final Handler mHandler;
    private final Context mContext;
    private final MemoryConfig mConfig;
    private long mStartTime;
    private long mNextReportTime;
    private int mNextReportFactor;
    private boolean mIsForeground = true;
    private String mShowingActivity;
    private HashMap<Integer, Long> mTrimedFlags;
    private final Runnable mDelayCheck = new Runnable() {
        @Override
        public void run() {
            detectAppMemoryInfo(false, 0);
        }
    };
    private final Runnable mNormalCheck = new Runnable() {
        @Override
        public void run() {
            detectAppMemoryInfo(false, 0);
        }
    };

    private final Application.ActivityLifecycleCallbacks mActivityLifecycleCallback = new Application.ActivityLifecycleCallbacks() {
        @Override
        public void onActivityCreated(Activity activity, Bundle bundle) {
        }

        @Override
        public void onActivityStarted(final Activity activity) {
        }

        @Override
        public void onActivityResumed(Activity activity) {
            onShow(activity);
        }

        @Override
        public void onActivityPaused(Activity activity) {

        }

        @Override
        public void onActivityStopped(Activity activity) {

        }

        @Override
        public void onActivitySaveInstanceState(Activity activity, Bundle bundle) {

        }

        @Override
        public void onActivityDestroyed(Activity activity) {

        }
    };

    private final ComponentCallbacks2 mComponentCallback = new ComponentCallbacks2() {
        @Override
        public void onTrimMemory(final int i) {
            switch (i) {
                case TRIM_MEMORY_RUNNING_CRITICAL:
                case TRIM_MEMORY_COMPLETE: {
                    long memFree = DeviceUtil.getMemFree(mContext);
                    long threshold = DeviceUtil.getLowMemoryThresold(mContext);
                    if (memFree >= (2 * threshold)) {
                        MatrixLog.i(TAG, "onTrimMemory level:%d, but memFree > 2*threshold, memFree:%d, threshold:%d", i, memFree, threshold);
                        return;
                    }
                }
                break;
                default:
                    return;
            }

            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    detectAppMemoryInfo(true, i);
                }
            });
        }

        @Override
        public void onConfigurationChanged(Configuration configuration) {

        }

        @Override
        public void onLowMemory() {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    detectAppMemoryInfo(true, LOW_MEMORY_TRIM);
                }
            });
        }
    };

    public MemoryCanaryCore(MemoryCanaryPlugin plugin) {
        mHandler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper());
        mPlugin = plugin;
        mContext = plugin.getApplication();
        mConfig = plugin.getConfig();
    }

    public void start() {
        mLevel = DeviceUtil.getLevel(mContext);
        if (!isApiLevelOk() || mLevel == DeviceUtil.LEVEL.LOW || mLevel == DeviceUtil.LEVEL.BAD || mLevel == DeviceUtil.LEVEL.UN_KNOW) {
            mIsOpen = false;
            return;
        }

        mIsOpen = true;

        mStartTime = System.currentTimeMillis();
        mNextReportFactor = 1;
        mNextReportTime = mStartTime + getNextDelay() - 5000;
        MatrixLog.d(TAG, "next report delay:%d, starttime:%d", getNextDelay(), mStartTime);
        mHandler.postDelayed(mDelayCheck, getNextDelay());
        mTrimedFlags = new HashMap<>();

        //all in kb
        mTotalMemory = DeviceUtil.getTotalMemory(mContext) / 1024;
        mLowMemoryThreshold = DeviceUtil.getLowMemoryThresold(mContext) / 1024;
        mMemoryClass = DeviceUtil.getMemoryClass(mContext);
        if (mLowMemoryThreshold >= mTotalMemory || mLowMemoryThreshold <= 0 || mMemoryClass <= (100 * 1024) || mTotalMemory <= 0) {
            mIsOpen = false;
            return;
        }

        ((Application) mContext).registerActivityLifecycleCallbacks(mActivityLifecycleCallback);
        mContext.registerComponentCallbacks(mComponentCallback);
    }

    public void stop() {
        ((Application) mContext).unregisterActivityLifecycleCallbacks(mActivityLifecycleCallback);
        mContext.unregisterComponentCallbacks(mComponentCallback);
        mIsOpen = false;
    }

    @Override
    public void onDetectIssue(Issue issue) {
        MatrixLog.i(TAG, "detected memory json:" + issue.getContent().toString());
        mPlugin.onDetectIssue(issue);
    }

    private void onShow(final Activity activity) {
        if (!mIsOpen) {
            return;
        }

        mHandler.removeCallbacks(mNormalCheck);

        MatrixLog.d(TAG, "activity on show:" + activity.getClass().getSimpleName());
        mShowingActivity = activity.getClass().getSimpleName();

        mHandler.postDelayed(mNormalCheck, 1000);
    }

    private void detectAppMemoryInfo(boolean bDetectAll, int flag) {
        if(!mIsOpen)
            return;

        if (!bDetectAll) {
            detectRuntimeMemoryInfo();
        } else {
            detectAppMemoryInfoImpl(flag);
        }
    }

    private void detectRuntimeMemoryInfo() {
        long dalvikHeap = DeviceUtil.getDalvikHeap();
        long nativeHeap = DeviceUtil.getNativeHeap();
        MatrixLog.d(TAG, "current dalvik heap:" + dalvikHeap + ", native heap:" + nativeHeap);

        double ratio = ((double) dalvikHeap / (double) (mMemoryClass));
        if (ratio >= mConfig.getThreshold()) {
            detectAppMemoryInfoImpl(LOW_JAVA_HEAP_FLAG);
            return;
        }
        ratio = ((double) nativeHeap / (double) NATIVE_HEAP_LIMIT);
        if (ratio >= mConfig.getThreshold()) {
            detectAppMemoryInfoImpl(LOW_NATIVE_HEAP_FLAG);
            return;
        }
        ratio = ((double) DeviceUtil.getVmSize() / (double) VMSIZE_LIMIT);
        if (ratio >= mConfig.getThreshold()) {
            detectAppMemoryInfoImpl(LOW_VMSIZE_FLAG);
            return;
        }

        long curr = System.currentTimeMillis();
        if (curr < mNextReportTime) {
            return;
        }

        Issue issue = new Issue();
        issue.setTag(mPlugin.getTag());
        issue.setType(SharePluginInfo.IssueType.ISSUE_INFO);
        JSONObject json = new JSONObject();
        issue.setContent(json);

        try {
            long span = System.currentTimeMillis() - mStartTime;
            if (span < 0) {
                MatrixLog.e(TAG, "wrong time, curr:%d, start:%d", System.currentTimeMillis(), mStartTime);
                return;
            }

            json.put(SharePluginInfo.ISSUE_STARTED_TIME, (int) (System.currentTimeMillis() - mStartTime) / (60 * 1000));

            long start = System.currentTimeMillis();
            Debug.MemoryInfo memoryInfo = DeviceUtil.getAppMemory(mContext);
            if (memoryInfo != null) {
                long cost = System.currentTimeMillis() - start;
                MatrixLog.i(TAG, "get app memory cost:" + cost);
                if(cost > MAX_COST) {
                    mIsOpen = false;
                    return;
                }
                MatrixMemoryInfo appInfo = new MatrixMemoryInfo(mShowingActivity);
                makeMatrixMemoryInfo(memoryInfo, appInfo);
                fillMemoryInfo(json, appInfo, SharePluginInfo.ISSUE_APP_MEM, mShowingActivity);
                json.put(SharePluginInfo.ISSUE_FOREGROUND, mIsForeground ? 1 : 0);
            }
        } catch (Exception e) {
            MatrixLog.e(TAG, "normal info json exception:" + e.toString());
            return;
        }
        onDetectIssue(issue);

        mNextReportFactor += 1;
        long delay = getNextDelay();
        delay = Math.min(delay, MAX_STEP);
        mNextReportTime = System.currentTimeMillis() + delay - 5000;
        mHandler.removeCallbacks(mDelayCheck);
        mHandler.postDelayed(mDelayCheck, delay);
    }

    //call when memory low
    private void detectAppMemoryInfoImpl(int flag) {
        if (flag == 0) {
            return;
        }

        if (mTrimedFlags.containsKey(flag) && (System.currentTimeMillis() - mTrimedFlags.get(flag)) < TRIM_MEMORY_SPAN) {
            MatrixLog.w(TAG, "trim memory too freq activity:%s, flag:%d", mShowingActivity, flag);
            return;
        }

        long start = System.currentTimeMillis();
        Debug.MemoryInfo memoryInfo = DeviceUtil.getAppMemory(mContext);
        if (memoryInfo == null) {
            return;
        }

        long cost = System.currentTimeMillis() - start;
        MatrixLog.i(TAG, "get app memory cost:" + cost);
        if(cost > MAX_COST) {
            mIsOpen = false;
            return;
        }

        //ontrimmemory or use too much memory
        MatrixMemoryInfo matrixMemoryInfo = new MatrixMemoryInfo(mShowingActivity);
        makeMatrixMemoryInfo(memoryInfo, matrixMemoryInfo);

        Issue issue = new Issue();
        issue.setTag(mPlugin.getTag());
        issue.setType(SharePluginInfo.IssueType.ISSUE_TRIM);
        JSONObject json = new JSONObject();
        issue.setContent(json);
        try {
            json.put(SharePluginInfo.ISSUE_SYSTEM_MEMORY, mTotalMemory);
            json.put(SharePluginInfo.ISSUE_THRESHOLD, mLowMemoryThreshold);
            json.put(SharePluginInfo.ISSUE_MEM_CLASS, mMemoryClass);
            json.put(SharePluginInfo.ISSUE_AVAILABLE, DeviceUtil.getAvailMemory(mContext));
            fillMemoryInfo(json, matrixMemoryInfo, SharePluginInfo.ISSUE_APP_MEM, mShowingActivity);
            json.put(SharePluginInfo.ISSUE_FOREGROUND, mIsForeground ? 1 : 0);
            json.put(SharePluginInfo.ISSUE_TRIM_FLAG, flag);
            json.put(SharePluginInfo.ISSUE_MEM_FREE, DeviceUtil.getMemFree(mContext));
            json.put(SharePluginInfo.ISSUE_IS_LOW, DeviceUtil.isLowMemory(mContext));
            mTrimedFlags.put(flag, System.currentTimeMillis());
        } catch (Exception e) {
            MatrixLog.e(TAG, "trim memory json exception:" + e.toString());
            return;
        }

        onDetectIssue(issue);
        return;
    }

    private void fillMemoryInfo(JSONObject json, final MatrixMemoryInfo matrixMemoryInfo, final String tag, final String activity) throws JSONException {
        JSONObject inner = new JSONObject();
        inner.put(SharePluginInfo.ISSUE_APP_PSS, matrixMemoryInfo.mTotalPss);
        inner.put(SharePluginInfo.ISSUE_APP_USS, matrixMemoryInfo.mTotalUss);
        inner.put(SharePluginInfo.ISSUE_APP_JAVA, matrixMemoryInfo.mJavaHeap);
        inner.put(SharePluginInfo.ISSUE_APP_NATIVE, matrixMemoryInfo.mNativePss);
        inner.put(SharePluginInfo.ISSUE_APP_GRAPHICS, matrixMemoryInfo.mGraphics);
        inner.put(SharePluginInfo.ISSUE_APP_STACK, matrixMemoryInfo.mStack);
        inner.put(SharePluginInfo.ISSUE_APP_CODE, matrixMemoryInfo.mCode);
        inner.put(SharePluginInfo.ISSUE_APP_OTHER, matrixMemoryInfo.mOther);
        inner.put(SharePluginInfo.ISSUE_DALVIK_HEAP, matrixMemoryInfo.mDalvikHeap);
        inner.put(SharePluginInfo.ISSUE_NATIVE_HEAP, matrixMemoryInfo.mNativeHeap);
        inner.put(SharePluginInfo.ISSUE_VMSIZE, DeviceUtil.getVmSize());
        if (!activity.isEmpty()) {
            inner.put(SharePluginInfo.ISSUE_ACTIVITY, activity);
        }
        json.put(tag, inner);
    }

    private boolean isApiLevelOk() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;
    }

    private void makeMatrixMemoryInfo(final Debug.MemoryInfo memoryInfo, MatrixMemoryInfo matrixMemoryInfo) {
        matrixMemoryInfo.mJavaHeap = DebugMemoryInfoUtil.getMemoryStat(JAVA_HEAP, memoryInfo);
        if (matrixMemoryInfo.mJavaHeap == -1) {
            return;
        }
        matrixMemoryInfo.mNativePss = DebugMemoryInfoUtil.getMemoryStat(NATIVE_HEAP, memoryInfo);
        if (matrixMemoryInfo.mNativePss == -1) {
            return;
        }
        matrixMemoryInfo.mCode = DebugMemoryInfoUtil.getMemoryStat(CODE_MEM, memoryInfo);
        if (matrixMemoryInfo.mCode == -1) {
            return;
        }
        matrixMemoryInfo.mStack = DebugMemoryInfoUtil.getMemoryStat(STACK_MEM, memoryInfo);
        if (matrixMemoryInfo.mStack == -1) {
            return;
        }
        matrixMemoryInfo.mGraphics = DebugMemoryInfoUtil.getMemoryStat(GRAPHICS_MEM, memoryInfo);
        if (matrixMemoryInfo.mGraphics == -1) {
            return;
        }
        matrixMemoryInfo.mOther = DebugMemoryInfoUtil.getMemoryStat(PRIVATE_OTHER, memoryInfo);
        if (matrixMemoryInfo.mOther == -1) {
            return;
        }
        matrixMemoryInfo.mTotalPss = DebugMemoryInfoUtil.getMemoryStat(TOTAL_PSS, memoryInfo);
        if (matrixMemoryInfo.mTotalPss == -1) {
            return;
        }
        matrixMemoryInfo.mTotalUss = DebugMemoryInfoUtil.getTotalUss(memoryInfo);

        matrixMemoryInfo.mDalvikHeap = (int) DeviceUtil.getDalvikHeap();
        matrixMemoryInfo.mNativeHeap = (int) DeviceUtil.getNativeHeap();

        MatrixLog.i(TAG, "activity:" + mShowingActivity + ", totalpss:" + matrixMemoryInfo.mTotalPss + ", uss:" + matrixMemoryInfo.mTotalUss + ", java:" + matrixMemoryInfo.mJavaHeap
                + " , Native:" + matrixMemoryInfo.mNativePss + ", code:" + matrixMemoryInfo.mCode + ", stack:" + matrixMemoryInfo.mStack
                + ", Graphics:" + matrixMemoryInfo.mGraphics + ", other:" + matrixMemoryInfo.mOther);
    }

    private long getNextDelay() {
        if (mNextReportFactor >= 8) {
            return 30 * STEP_FACTOR;
        }

        long start = System.currentTimeMillis();
        long delay = (getFib(mNextReportFactor) - getFib(mNextReportFactor - 1)) * STEP_FACTOR;
        long cost = System.currentTimeMillis() - start;
        if (cost > 1000) {
            MatrixLog.e(TAG, "[getNextDelay] cost time[%s] too long!", cost);
        }
        return delay;
    }

    private int getFib(int n) {
        if (n <= 0) {
            return 0;
        } else if (n == 1) {
            return 1;
        } else if (n == 2) {
            return 2;
        } else if(n >= 8) {
            return 30;
        } else {
            return getFib(n - 1) + getFib(n - 2);
        }
    }

    public JSONObject getJsonInfo() {
        JSONObject json = new JSONObject();

        long dalvikHeap = DeviceUtil.getDalvikHeap();
        long nativeHeap = DeviceUtil.getNativeHeap();
        try {
            json.put(PluginShareConstants.MemoryCanaryShareKeys.DALVIK_HEAP, dalvikHeap);
            json.put(PluginShareConstants.MemoryCanaryShareKeys.NATIVE_HEAP, nativeHeap);
            json.put(PluginShareConstants.MemoryCanaryShareKeys.SYSTEM_MEMORY, mTotalMemory);
            json.put(PluginShareConstants.MemoryCanaryShareKeys.MEM_CLASS, mMemoryClass);
            json.put(PluginShareConstants.MemoryCanaryShareKeys.AVAILABLE, DeviceUtil.getAvailMemory(mContext));
            json.put(PluginShareConstants.MemoryCanaryShareKeys.MEM_FREE, DeviceUtil.getMemFree(mContext));
            json.put(PluginShareConstants.MemoryCanaryShareKeys.IS_LOW, DeviceUtil.isLowMemory(mContext));
            json.put(PluginShareConstants.MemoryCanaryShareKeys.VM_SIZE, DeviceUtil.getVmSize());
        } catch (JSONException e) {
            MatrixLog.e(TAG, "getJsonInfo exception:" + e.getMessage());
        }

        return json;
    }
}
