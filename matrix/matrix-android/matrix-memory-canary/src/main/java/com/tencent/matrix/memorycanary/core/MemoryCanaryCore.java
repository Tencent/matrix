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

import com.tencent.matrix.memorycanary.IFragmentActivity;
import com.tencent.matrix.memorycanary.MemoryCanaryPlugin;
import com.tencent.matrix.memorycanary.config.MemoryConfig;
import com.tencent.matrix.memorycanary.config.SharePluginInfo;
import com.tencent.matrix.memorycanary.util.DebugMemoryInfoUtil;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.report.IssuePublisher;
import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

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
import java.util.HashSet;

/**
 * Created by astrozhou on 2018/9/18.
 */
public class MemoryCanaryCore implements IssuePublisher.OnIssueDetectListener {

    public static class MatrixMemoryInfo {
        MatrixMemoryInfo(int activity) {
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
        public int mActivity;
    }

    private static final String TAG = "MemoryCanaryCore";
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

    private static final int STEP_FACTOR = 60 * 1000;
    private static final int MAX_STEP = 30 * STEP_FACTOR;
    private static final int NATIVE_HEAP_LIMIT = 500 * 1024;
    private static final int TRIM_MEMORY_SPAN = 10 * 60 * 1000;

    private final MemoryCanaryPlugin mPlugin;
    private boolean mIsOpen = false;
    private DeviceUtil.LEVEL mLevel;
    private static long mTotalMemory = 0;
    private static long mLowMemoryThreshold = 0;
    private static int mMemoryClass = 0;
    private final Handler mHandler;
    private final Context mContext;
    private final MemoryConfig mConfig;
    private final long mStartTime;
    private long mNextReportTime;
    private int mNextReportFactor;
    private long mLastTime;
    private final HashMap<Integer, String> mActivityMap;
    private final HashSet<String> mSpecialActivityName;
    private final HashSet<Integer> mSpecialActivity;
    private boolean mIsForeground = true;
    private int mShowingActivity = 0;
    private int mDetectFibFactor = 1;
    private final HashMap<Integer, Long> mTrimedFlags;
    private final Runnable mDelayCheck = new Runnable() {
        @Override
        public void run() {
            detectAppMemoryInfo(0, false, 0);
        }
    };

    private final Application.ActivityLifecycleCallbacks mActivityLifecycleCallback = new Application.ActivityLifecycleCallbacks() {
        @Override
        public void onActivityCreated(Activity activity, Bundle bundle) {
            if (!mActivityMap.containsKey(activity.getClass().hashCode())) {
                mActivityMap.put(activity.getClass().hashCode(), activity.getClass().getSimpleName());
                if (mSpecialActivityName.contains(activity.getClass().getSimpleName())) {
                    mSpecialActivity.add(activity.getClass().hashCode());
                }
            }

            MatrixLog.d(TAG, "activity create:" + activity.getClass().getSimpleName());
        }

        @Override
        public void onActivityStarted(final Activity activity) {
            onShow(activity);
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
                    break;
                case TRIM_MEMORY_COMPLETE:
                    break;
                default:
                    return;
            }

            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    detectAppMemoryInfo(mShowingActivity, true, i);
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
                    detectAppMemoryInfo(mShowingActivity, true, LOW_MEMORY_TRIM);
                }
            });
        }
    };

    public MemoryCanaryCore(MemoryCanaryPlugin plugin) {
        mHandler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper());
        mPlugin = plugin;
        mContext = plugin.getApplication();
        mConfig = plugin.getConfig();

        mStartTime = System.currentTimeMillis();
        mNextReportFactor = 1;
        mNextReportTime = mStartTime + getFib(mNextReportFactor) * STEP_FACTOR;
        mHandler.postDelayed(mDelayCheck, (getFib(mNextReportFactor) * STEP_FACTOR));
        mActivityMap = new HashMap<>();
        mSpecialActivity = new HashSet<>();
        mSpecialActivityName = new HashSet<>();
        mTrimedFlags = new HashMap<>();
        mLastTime = 0;
    }

    public void addSpecial(final String activity) {
        mSpecialActivityName.add(activity);
    }

    private void clear() {
        mLastTime = 0;
        mActivityMap.clear();
    }

    public void start() {
        mLevel = DeviceUtil.getLevel(mContext);
        if (!isApiLevelOk() || mLevel == DeviceUtil.LEVEL.LOW || mLevel == DeviceUtil.LEVEL.BAD || mLevel == DeviceUtil.LEVEL.UN_KNOW) {
            mIsOpen = false;
            return;
        }

        mIsOpen = true;

        //all in kb
        mTotalMemory = DeviceUtil.getTotalMemory(mContext) / 1024;
        mLowMemoryThreshold = DeviceUtil.getLowMemoryThresold(mContext) / 1024;
        mMemoryClass = DeviceUtil.getMemoryClass(mContext);
        if (mLowMemoryThreshold >= mTotalMemory || mLowMemoryThreshold == 0) {
            mIsOpen = false;
        }

        ((Application) mContext).registerActivityLifecycleCallbacks(mActivityLifecycleCallback);
        mContext.registerComponentCallbacks(mComponentCallback);
    }

    public void stop() {
        ((Application) mContext).unregisterActivityLifecycleCallbacks(mActivityLifecycleCallback);
        mContext.unregisterComponentCallbacks(mComponentCallback);

        if (!mIsOpen) {
            return;
        }

        clear();
    }

    public void onForeground(boolean bIsForeground) {
        mIsForeground = bIsForeground;
        if (!mIsOpen) {
            return;
        }

        if (!mIsForeground) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    mDetectFibFactor = 1;
                    detectAppMemoryInfo(0, false, 0);
                }
            });
        }
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

        int hashCode = activity.getClass().hashCode();
        if (mSpecialActivity.contains(hashCode)) {
            if (!(activity instanceof IFragmentActivity)) {
                return;
            }

            hashCode = ((IFragmentActivity) activity).hashCodeOfCurrentFragment();
            String name = ((IFragmentActivity) activity).fragmentName();
            if (!mActivityMap.containsKey(hashCode)) {
                mActivityMap.put(hashCode, name);
            }
        }
        if (mShowingActivity == hashCode) {
            return;
        }

        mHandler.removeCallbacksAndMessages(null);

        MatrixLog.d(TAG, "activity on show:" + activity.getClass().getSimpleName());

        mShowingActivity = hashCode;
        mDetectFibFactor = 1;

        if ((System.currentTimeMillis() - mLastTime) < ((mLevel == DeviceUtil.LEVEL.BEST || mLevel == DeviceUtil.LEVEL.HIGH) ? mConfig.getHighMinSpan() : mConfig.getMiddleMinSpan())) {
            return; //too frequent, ignore once
        }

        final int hashCodeTmp = hashCode;
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                detectAppMemoryInfo(hashCodeTmp, false, 0);
            }
        });
        mLastTime = System.currentTimeMillis();
    }

    private void detectAppMemoryInfo(final int activity, boolean bDetectAll, int flag) {
        if (!bDetectAll) {
            detectRuntimeMemoryInfo(activity);
        } else {
            detectAppMemoryInfoImpl(activity, flag);
        }

        if (!bDetectAll && activity == mShowingActivity && (mIsForeground || mDetectFibFactor <= 8)) {
            long nextDetect = getFib(mDetectFibFactor) * 1000L;
            mDetectFibFactor += 1;
            mHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    detectAppMemoryInfo(activity, false, 0);
                }
            }, nextDetect);
        }
    }

    private void detectRuntimeMemoryInfo(int activity) {
        long dalvikHeap = DeviceUtil.getDalvikHeap();
        long nativeHeap = DeviceUtil.getNativeHeap();
        //MatrixLog.d(TAG, "current dalvik heap:" + dalvikHeap + ", native heap:" + nativeHeap);
        double ratio = ((double) dalvikHeap / (double) (mMemoryClass));
        if (ratio >= mConfig.getThreshold()) {
            detectAppMemoryInfoImpl(activity, LOW_JAVA_HEAP_FLAG);
            return;
        }
        ratio = ((double) nativeHeap / (double) NATIVE_HEAP_LIMIT);
        if (ratio >= mConfig.getThreshold()) {
            detectAppMemoryInfoImpl(activity, LOW_NATIVE_HEAP_FLAG);
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
            json.put(SharePluginInfo.ISSUE_STARTED_TIME, (int) (System.currentTimeMillis() - mStartTime) / (60 * 1000));

            long start = System.currentTimeMillis();
            Debug.MemoryInfo memoryInfo = DeviceUtil.getAppMemory(mContext);
            if (memoryInfo != null) {
                long cost = System.currentTimeMillis() - start;
                MatrixLog.i(TAG, "get app memory cost:" + cost);
                MatrixMemoryInfo appInfo = new MatrixMemoryInfo(activity);
                makeMatrixMemoryInfo(memoryInfo, appInfo);
                fillMemoryInfo(json, appInfo, SharePluginInfo.ISSUE_APP_MEM, mActivityMap.containsKey(activity) ? mActivityMap.get(activity) : "");
                json.put(SharePluginInfo.ISSUE_FOREGROUND, mIsForeground ? 1 : 0);
            }
        } catch (Exception e) {
            MatrixLog.e(TAG, "normal info json exception:" + e.toString());
            return;
        }
        onDetectIssue(issue);

        mNextReportFactor += 1;
        int factor = getFib(mNextReportFactor) * STEP_FACTOR;
        factor = Math.min(factor, MAX_STEP);
        mNextReportTime += factor;
        mHandler.removeCallbacks(mDelayCheck);
        mHandler.postDelayed(mDelayCheck, factor);
    }

    //call when memory low
    private void detectAppMemoryInfoImpl(int activity, int flag) {
        if (flag == 0) {
            return;
        }

        if (mTrimedFlags.containsKey(flag) && (System.currentTimeMillis() - mTrimedFlags.get(flag)) < TRIM_MEMORY_SPAN) {
            MatrixLog.w(TAG, "trim memory too freq activity:%d, flag:%d", activity, flag);
            return;
        }

        long start = System.currentTimeMillis();
        Debug.MemoryInfo memoryInfo = DeviceUtil.getAppMemory(mContext);
        if (memoryInfo == null) {
            return;
        }

        long cost = System.currentTimeMillis() - start;
        MatrixLog.i(TAG, "get app memory cost:" + cost);

        //ontrimmemory or use too much memory
        MatrixMemoryInfo matrixMemoryInfo = new MatrixMemoryInfo(activity);
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
            fillMemoryInfo(json, matrixMemoryInfo, SharePluginInfo.ISSUE_APP_MEM, mActivityMap.containsKey(activity) ? mActivityMap.get(activity) : "");
            json.put(SharePluginInfo.ISSUE_FOREGROUND, mIsForeground ? 1 : 0);
            json.put(SharePluginInfo.ISSUE_TRIM_FLAG, flag);
            json.put(SharePluginInfo.ISSUE_MEM_FREE, DeviceUtil.getMemFree());
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

        String name = mActivityMap.containsKey(matrixMemoryInfo.mActivity) ? mActivityMap.get(matrixMemoryInfo.mActivity) : "null";
        MatrixLog.i(TAG, "activity:" + name + ", totalpss:" + matrixMemoryInfo.mTotalPss + ", uss:" + matrixMemoryInfo.mTotalUss + ", java:" + matrixMemoryInfo.mJavaHeap
                + " , Native:" + matrixMemoryInfo.mNativePss + ", code:" + matrixMemoryInfo.mCode + ", stack:" + matrixMemoryInfo.mStack
                + ", Graphics:" + matrixMemoryInfo.mGraphics + ", other:" + matrixMemoryInfo.mOther);
    }

    private int getFib(int n) {
        if (n < 0) {
            return -1;
        } else if (n == 0) {
            return 0;
        } else if (n == 1 || n == 2) {
            return 1;
        } else {
            return getFib(n - 1) + getFib(n - 2);
        }
    }
}
