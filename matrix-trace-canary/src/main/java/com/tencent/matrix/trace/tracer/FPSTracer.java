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
import android.support.v4.app.Fragment;
import android.util.SparseArray;
import android.view.ViewTreeObserver;

import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.SharePluginInfo;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.schedule.LazyScheduler;
import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.LinkedList;

/**
 * Created by caichongyang on 2017/6/5.
 * <p> this class is responsible for collecting as well as reporting FPS of activity.
 * </p>
 */

public class FPSTracer extends BaseTracer implements LazyScheduler.ILazyTask, ViewTreeObserver.OnDrawListener {

    private static final String TAG = "Matrix.FPSTracer";

    private static final int OFFSET_TO_MS = 100;
    private static final int FACTOR = Constants.TIME_MILLIS_TO_NANO / OFFSET_TO_MS;
    private final TraceConfig mTraceConfig;
    private boolean isDrawing = false;
    private boolean isInvild = false;
    private HashMap<String, Integer> mSceneToSceneIdMap;
    private SparseArray<String> mSceneIdToSceneMap;
    private LinkedList<Integer> mFrameDataList;
    private SparseArray<LinkedList<Integer>> mPendingReportSet;
    private LazyScheduler mLazyScheduler;

    public FPSTracer(TracePlugin plugin, TraceConfig config) {
        super(plugin);
        this.mTraceConfig = config;
        this.mFrameDataList = new LinkedList<>();
        this.mSceneToSceneIdMap = new HashMap<>();
        this.mSceneIdToSceneMap = new SparseArray<>();
        this.mPendingReportSet = new SparseArray<>();
        this.mLazyScheduler = new LazyScheduler(MatrixHandlerThread.getDefaultHandlerThread(), config.getFPSReportInterval());
    }

    @Override
    public void onActivityCreated(Activity activity) {
        super.onActivityCreated(activity);
    }

    /**
     * When the application comes to the foreground,it will be called.
     *
     * @param activity
     */
    @Override
    public void onFront(Activity activity) {
        super.onFront(activity);
        if (null != mLazyScheduler) {
            mLazyScheduler.cancel();
            this.mLazyScheduler.setUp(this, true);
        }
    }

    /**
     * When the application comes to the background,it will be called.
     *
     * @param activity
     */
    @Override
    public void onBackground(Activity activity) {
        super.onBackground(activity);
        if (null != mLazyScheduler) {
            mLazyScheduler.cancel();
            this.mLazyScheduler.setUp(this, false);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (null != mLazyScheduler) {
            mLazyScheduler.setOff();
            mLazyScheduler = null;
        }
        if (null != mSceneToSceneIdMap) {
            mSceneToSceneIdMap.clear();
            mSceneToSceneIdMap = null;
        }
        if (null != mSceneIdToSceneMap) {
            mSceneIdToSceneMap.clear();
            mSceneIdToSceneMap = null;
        }
        if (null != mFrameDataList) {
            mFrameDataList.clear();
            mFrameDataList = null;
        }
        if (null != mPendingReportSet) {
            mPendingReportSet.clear();
            mPendingReportSet = null;
        }
    }

    @Override
    public void onDraw() {
        isDrawing = true;
    }

    @Override
    protected String getTag() {
        return SharePluginInfo.TAG_PLUGIN_FPS;
    }

    /**
     * when the activity was change,it will be called.
     *
     * @param activity
     * @param fragment
     */
    @Override
    public void onChange(final Activity activity, final Fragment fragment) {
        if (null == activity || null == fragment) {
            MatrixLog.e(TAG, "Empty Parameters");
            return;
        }

        String lastScene = getScene();
        super.onChange(activity, fragment);
        MatrixLog.i(TAG, "[onChange] activity:%s lastActivity:%s", activity.getClass().getSimpleName(), lastScene == null ? "" : lastScene);
        if (null != activity.getWindow() && null != activity.getWindow().getDecorView()) {
            activity.getWindow().getDecorView().post(new Runnable() {
                @Override
                public void run() {
                    activity.getWindow().getDecorView().getViewTreeObserver().removeOnDrawListener(FPSTracer.this);
                    activity.getWindow().getDecorView().getViewTreeObserver().addOnDrawListener(FPSTracer.this);
                }
            });
        }

    }

    /**
     * it's time to report,it will be called.
     */
    @Override
    public void onTimeExpire() {
        doReport();
    }

    /**
     * when the device's Vsync is coming,it will be called.
     * use {@link com.tencent.matrix.trace.core.FrameBeat}
     *
     * @param lastFrameNanos
     * @param frameNanos
     */
    @Override
    public void doFrame(long lastFrameNanos, long frameNanos) {
        if (!isInvild && !isBackground() && isDrawing && isEnterAnimationComplete() && mTraceConfig.isTargetScene(getScene())) {
            handleDoFrame(lastFrameNanos, frameNanos, getScene());
        }
        isDrawing = false;
    }

    @Override
    public void onActivityResume(Activity activity) {
        super.onActivityResume(activity);
        this.isInvild = false;
    }

    @Override
    public void onActivityPause(Activity activity) {
        super.onActivityPause(activity);
        this.isInvild = true;
    }

    /**
     * collect ths info about FPS
     *
     * @param lastFrameNanos
     * @param frameNanos
     * @param scene
     */
    private void handleDoFrame(long lastFrameNanos, long frameNanos, String scene) {

        int sceneId;
        if (mSceneToSceneIdMap.containsKey(scene)) {
            sceneId = mSceneToSceneIdMap.get(scene);
        } else {
            sceneId = mSceneToSceneIdMap.size() + 1;
            mSceneToSceneIdMap.put(scene, sceneId);
            mSceneIdToSceneMap.put(sceneId, scene);
        }
        int trueId = 0x0;
        trueId |= sceneId;
        trueId = trueId << 22;
        trueId |= (((frameNanos - lastFrameNanos) / FACTOR) & 0x3FFFFF);

        synchronized (this.getClass()) {
            mFrameDataList.add(trueId);
        }
    }

    /**
     * report FPS
     */
    private void doReport() {
        LinkedList<Integer> reportList;
        synchronized (this.getClass()) {
            if (mFrameDataList.isEmpty()) {
//                MatrixLog.d(TAG, "fps report analyze, frame date list is empty, just ignore");
                return;
            }
            reportList = mFrameDataList;
            mFrameDataList = new LinkedList<>();
        }

        for (int trueId : reportList) {
            int scene = trueId >> 22;
            int durTime = trueId & 0x3FFFFF;
            LinkedList<Integer> list = mPendingReportSet.get(scene);
            if (null == list) {
                list = new LinkedList<>();
                mPendingReportSet.put(scene, list);
            }
            list.add(durTime);
        }
        reportList.clear();

        for (int i = 0; i < mPendingReportSet.size(); i++) {
            int key = mPendingReportSet.keyAt(i);
            LinkedList<Integer> list = mPendingReportSet.get(key);
            if (null == list) {
                continue;
            }
            int sumTime = 0;
            int markIndex = 0;
            int count = 0;

            int[] dropLevel = new int[DropStatus.values().length]; // record the level of frames dropped each time
            int[] dropSum = new int[DropStatus.values().length]; // record the sum of frames dropped each time
            int refreshRate = (int) Constants.DEFAULT_DEVICE_REFRESH_RATE * OFFSET_TO_MS;
            for (Integer period : list) {
                sumTime += period;
                count++;
                int tmp = period / refreshRate - 1;
                if (tmp >= Constants.DEFAULT_DROPPED_FROZEN) {
                    dropLevel[DropStatus.DROPPED_FROZEN.index]++;
                    dropSum[DropStatus.DROPPED_FROZEN.index] += tmp;
                } else if (tmp >= Constants.DEFAULT_DROPPED_HIGH) {
                    dropLevel[DropStatus.DROPPED_HIGH.index]++;
                    dropSum[DropStatus.DROPPED_HIGH.index] += tmp;
                } else if (tmp >= Constants.DEFAULT_DROPPED_MIDDLE) {
                    dropLevel[DropStatus.DROPPED_MIDDLE.index]++;
                    dropSum[DropStatus.DROPPED_MIDDLE.index] += tmp;
                } else if (tmp >= Constants.DEFAULT_DROPPED_NORMAL) {
                    dropLevel[DropStatus.DROPPED_NORMAL.index]++;
                    dropSum[DropStatus.DROPPED_NORMAL.index] += tmp;
                } else {
                    dropLevel[DropStatus.DROPPED_BEST.index]++;
                    dropSum[DropStatus.DROPPED_BEST.index] += (tmp < 0 ? 0 : tmp);
                }

                if (sumTime >= mTraceConfig.getTimeSliceMs() * OFFSET_TO_MS) { // if it reaches report time
                    float fps = Math.min(60.f, 1000.f * OFFSET_TO_MS * (count - markIndex) / sumTime);
                    MatrixLog.i(TAG, "scene:%s fps:%s sumTime:%s [%s:%s]", mSceneIdToSceneMap.get(key), fps, sumTime, count, markIndex);
                    try {
                        JSONObject dropLevelObject = new JSONObject();
                        dropLevelObject.put(DropStatus.DROPPED_FROZEN.name(), dropLevel[DropStatus.DROPPED_FROZEN.index]);
                        dropLevelObject.put(DropStatus.DROPPED_HIGH.name(), dropLevel[DropStatus.DROPPED_HIGH.index]);
                        dropLevelObject.put(DropStatus.DROPPED_MIDDLE.name(), dropLevel[DropStatus.DROPPED_MIDDLE.index]);
                        dropLevelObject.put(DropStatus.DROPPED_NORMAL.name(), dropLevel[DropStatus.DROPPED_NORMAL.index]);
                        dropLevelObject.put(DropStatus.DROPPED_BEST.name(), dropLevel[DropStatus.DROPPED_BEST.index]);

                        JSONObject dropSumObject = new JSONObject();
                        dropSumObject.put(DropStatus.DROPPED_FROZEN.name(), dropSum[DropStatus.DROPPED_FROZEN.index]);
                        dropSumObject.put(DropStatus.DROPPED_HIGH.name(), dropSum[DropStatus.DROPPED_HIGH.index]);
                        dropSumObject.put(DropStatus.DROPPED_MIDDLE.name(), dropSum[DropStatus.DROPPED_MIDDLE.index]);
                        dropSumObject.put(DropStatus.DROPPED_NORMAL.name(), dropSum[DropStatus.DROPPED_NORMAL.index]);
                        dropSumObject.put(DropStatus.DROPPED_BEST.name(), dropSum[DropStatus.DROPPED_BEST.index]);

                        JSONObject resultObject = new JSONObject();
                        resultObject = DeviceUtil.getDeviceInfo(resultObject, getPlugin().getApplication());

                        resultObject.put(SharePluginInfo.ISSUE_SCENE, mSceneIdToSceneMap.get(key));
                        resultObject.put(SharePluginInfo.ISSUE_DROP_LEVEL, dropLevelObject);
                        resultObject.put(SharePluginInfo.ISSUE_DROP_SUM, dropSumObject);
                        resultObject.put(SharePluginInfo.ISSUE_FPS, fps);
                        sendReport(resultObject);
                    } catch (JSONException e) {
                        MatrixLog.e(TAG, "json error", e);
                    }


                    dropLevel = new int[DropStatus.values().length];
                    dropSum = new int[DropStatus.values().length];
                    markIndex = count;
                    sumTime = 0;
                }
            }

            // delete has reported data
            if (markIndex > 0) {
                for (int index = 0; index < markIndex; index++) {
                    list.removeFirst();
                }
            }

            if (!list.isEmpty()) {
                MatrixLog.d(TAG, "[doReport] sumTime:[%sms < %sms], scene:[%s]", sumTime / OFFSET_TO_MS, mTraceConfig.getTimeSliceMs(), mSceneIdToSceneMap.get(key));
            }
        }
    }


    private enum DropStatus {
        DROPPED_FROZEN(4), DROPPED_HIGH(3), DROPPED_MIDDLE(2), DROPPED_NORMAL(1), DROPPED_BEST(0);
        int index;

        DropStatus(int index) {
            this.index = index;
        }

    }

}
