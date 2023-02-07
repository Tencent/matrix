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
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;
import android.view.FrameMetrics;
import android.view.Window;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.SharePluginInfo;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.core.UIThreadMonitor;
import com.tencent.matrix.trace.listeners.IActivityFrameListener;
import com.tencent.matrix.trace.listeners.IDoFrameListener;
import com.tencent.matrix.trace.listeners.IDropFrameListener;
import com.tencent.matrix.trace.listeners.IFrameListener;
import com.tencent.matrix.trace.listeners.LooperObserver;
import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public class FrameTracer extends Tracer implements Application.ActivityLifecycleCallbacks {
    private static final String TAG = "Matrix.FrameTracer";

    public final static int sdkInt = Build.VERSION.SDK_INT;
    public static float defaultRefreshRate = 60;

    private int droppedSum = 0;

    @Deprecated
    private long durationSum = 0;
    @Deprecated
    private final HashSet<IDoFrameListener> oldListeners = new HashSet<>();
    @Deprecated
    private long frameIntervalNs;
    @Deprecated
    private LooperObserver looperObserver = new LooperObserver() {
        @Override
        public void doFrame(String focusedActivity, long startNs, long endNs, boolean isVsyncFrame, long intendedFrameTimeNs, long inputCostNs, long animationCostNs, long traversalCostNs) {
            if (isForeground()) {
                notifyListener(focusedActivity, startNs, endNs, isVsyncFrame, intendedFrameTimeNs, inputCostNs, animationCostNs, traversalCostNs);
            }
        }

        @Deprecated
        private void notifyListener(final String focusedActivity, final long startNs, final long endNs, final boolean isVsyncFrame,
                                    final long intendedFrameTimeNs, final long inputCostNs, final long animationCostNs, final long traversalCostNs) {
            long traceBegin = System.currentTimeMillis();
            try {
                final long jitter = endNs - intendedFrameTimeNs;
                final int dropFrame = (int) (jitter / frameIntervalNs);
                if (oldDropFrameListener != null && dropFrame > dropFrameListenerThreshold) {
                    try {
                        if (MatrixUtil.getTopActivityName() != null) {
                            oldDropFrameListener.dropFrame(dropFrame, jitter, MatrixUtil.getTopActivityName());
                        }
                    } catch (Exception e) {
                        MatrixLog.e(TAG, "dropFrameListener error e:" + e.getMessage());
                    }
                }

                droppedSum += dropFrame;
                durationSum += Math.max(jitter, frameIntervalNs);

                synchronized (oldListeners) {
                    for (final IDoFrameListener listener : oldListeners) {
                        if (config.isDevEnv()) {
                            listener.time = SystemClock.uptimeMillis();
                        }
                        if (null != listener.getExecutor()) {
                            if (listener.getIntervalFrameReplay() > 0) {
                                listener.collect(focusedActivity, startNs, endNs, dropFrame, isVsyncFrame,
                                        intendedFrameTimeNs, inputCostNs, animationCostNs, traversalCostNs);
                            } else {
                                listener.getExecutor().execute(new Runnable() {
                                    @Override
                                    public void run() {
                                        listener.doFrameAsync(focusedActivity, startNs, endNs, dropFrame, isVsyncFrame,
                                                intendedFrameTimeNs, inputCostNs, animationCostNs, traversalCostNs);
                                    }
                                });
                            }
                        } else {
                            listener.doFrameSync(focusedActivity, startNs, endNs, dropFrame, isVsyncFrame,
                                    intendedFrameTimeNs, inputCostNs, animationCostNs, traversalCostNs);
                        }

                        if (config.isDevEnv()) {
                            listener.time = SystemClock.uptimeMillis() - listener.time;
                            MatrixLog.d(TAG, "[notifyListener] cost:%sms listener:%s", listener.time, listener);
                        }
                    }
                }
            } finally {
                long cost = System.currentTimeMillis() - traceBegin;
                if (config.isDebug() && cost > frameIntervalNs) {
                    MatrixLog.w(TAG, "[notifyListener] warm! maybe do heavy work in doFrameSync! size:%s cost:%sms", oldListeners.size(), cost);
                }
            }
        }
    };

    @Deprecated
    private DropFrameListener oldDropFrameListener;
    private IDropFrameListener dropFrameListener;
    private int dropFrameListenerThreshold = 0;

    private final TraceConfig config;
    private final HashSet<IFrameListener> listeners = new HashSet<>();
    private final long frozenThreshold;
    private final long highThreshold;
    private final long middleThreshold;
    private final long normalThreshold;
    ActivityFrameCollector activityFrameCollector;
    private final Map<Integer, Window.OnFrameMetricsAvailableListener> frameListenerMap = new ConcurrentHashMap<>();

    public FrameTracer(TraceConfig config) {
        this.config = config;
        this.frameIntervalNs = UIThreadMonitor.getMonitor().getFrameIntervalNanos();
        this.frozenThreshold = config.getFrozenThreshold();
        this.highThreshold = config.getHighThreshold();
        this.normalThreshold = config.getNormalThreshold();
        this.middleThreshold = config.getMiddleThreshold();

        MatrixLog.i(TAG, "[init] frameIntervalMs:%s isFPSEnable:%s", frameIntervalNs, config.isFPSEnable());
    }

    @Deprecated
    public void addListener(IDoFrameListener listener) {
        synchronized (oldListeners) {
            oldListeners.add(listener);
        }
    }

    @Deprecated
    public void removeListener(IDoFrameListener listener) {
        synchronized (oldListeners) {
            oldListeners.remove(listener);
        }
    }

    @RequiresApi(Build.VERSION_CODES.N)
    public void addListener(IFrameListener listener) {
        synchronized (listeners) {
            listeners.add(listener);
        }
    }

    @RequiresApi(Build.VERSION_CODES.N)
    public void removeListener(IFrameListener listener) {
        synchronized (listeners) {
            listeners.remove(listener);
        }
    }

    @RequiresApi(Build.VERSION_CODES.N)
    public void register(IActivityFrameListener listener) {
        if (activityFrameCollector != null) {
            activityFrameCollector.register(listener);
        }
    }

    @RequiresApi(Build.VERSION_CODES.N)
    public void unregister(IActivityFrameListener listener) {
        if (activityFrameCollector != null) {
            activityFrameCollector.unregister(listener);
        }
    }

    @Override
    public void onAlive() {
        super.onAlive();
        if (config.isFPSEnable()) {
            forceEnable();
        }
    }

    public void forceEnable() {
        MatrixLog.i(TAG, "forceEnable");
        if (sdkInt >= Build.VERSION_CODES.N) {
            Matrix.with().getApplication().registerActivityLifecycleCallbacks(this);
            activityFrameCollector = new ActivityFrameCollector();
            addListener(activityFrameCollector);
            register(new AllActivityFrameListener());
        } else {
            UIThreadMonitor.getMonitor().addObserver(looperObserver);
        }
    }

    public void forceDisable() {
        MatrixLog.i(TAG, "forceDisable");
        removeDropFrameListener();
        if (sdkInt >= Build.VERSION_CODES.N) {
            Matrix.with().getApplication().unregisterActivityLifecycleCallbacks(this);
            listeners.clear();
            frameListenerMap.clear();
        } else {
            UIThreadMonitor.getMonitor().removeObserver(looperObserver);
            oldListeners.clear();
        }
    }

    @Override
    public void onDead() {
        super.onDead();
        if (config.isFPSEnable()) {
            forceDisable();
        }
    }

    public int getDroppedSum() {
        return droppedSum;
    }

    @Deprecated
    public long getDurationSum() {
        return durationSum;
    }

    @RequiresApi(Build.VERSION_CODES.N)
    private class ActivityFrameCollector implements IFrameListener {

        private final Handler frameHandler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper());
        private final HashMap<String, ActivityFrameCollectItem> map = new HashMap<>();
        private final ArrayList<ActivityFrameCollectItem> list = new ArrayList<>();

        public synchronized void register(IActivityFrameListener listener) {
            if (listener.getIntervalMs() < 1 || listener.getThreshold() < 0) {
                MatrixLog.e(TAG, "Illegal value, intervalMs=%d, threshold=%d, activity=%s",
                        listener.getIntervalMs(), listener.getThreshold(), listener.getClass().getName());
                return;
            }
            String scene = listener.getName();
            ActivityFrameCollectItem collectItem = new ActivityFrameCollectItem(listener);
            if (scene == null || scene.isEmpty()) {
                list.add(collectItem);
            } else {
                map.put(scene, collectItem);
            }
        }

        public synchronized void unregister(@NonNull IActivityFrameListener listener) {
            String scene = listener.getName();
            if (scene == null || scene.isEmpty()) {
                for (int i = 0; i < list.size(); i++) {
                    if (list.get(i).listener == listener) {
                        list.remove(i);
                        break;
                    }
                }
            } else {
                map.remove(scene);
            }
        }


        @Override
        public void onFrameMetricsAvailable(final Activity activity, final FrameMetrics frameMetrics, final float droppedFrames, final float refreshRate) {
            frameHandler.post(new Runnable() {
                @Override
                public void run() {
                    synchronized (ActivityFrameCollector.this) {
                        String scene = activity.getClass().getName();
                        ActivityFrameCollectItem collectItem = map.get(scene);
                        if (collectItem != null) {
                            collectItem.append(activity, frameMetrics, droppedFrames, refreshRate);
                        }
                        for (ActivityFrameCollectItem frameCollectItem : list) {
                            frameCollectItem.append(activity, frameMetrics, droppedFrames, refreshRate);
                        }
                    }
                }
            });
        }
    }

    public enum DropStatus {
        DROPPED_BEST, DROPPED_NORMAL, DROPPED_MIDDLE, DROPPED_HIGH, DROPPED_FROZEN;
    }

    public enum FrameDuration {
        UNKNOWN_DELAY_DURATION, INPUT_HANDLING_DURATION, ANIMATION_DURATION, LAYOUT_MEASURE_DURATION,
        DRAW_DURATION, SYNC_DURATION, COMMAND_ISSUE_DURATION, SWAP_BUFFERS_DURATION, TOTAL_DURATION,
        FIRST_DRAW_FRAME, INTENDED_VSYNC_TIMESTAMP, VSYNC_TIMESTAMP, // not supported
        GPU_DURATION,
        DEADLINE, // not supported
    }


    @RequiresApi(Build.VERSION_CODES.N)
    private class ActivityFrameCollectItem {
        private final long[] durations = new long[FrameDuration.DEADLINE.ordinal()];
        private final int[] dropLevel = new int[DropStatus.values().length];
        private final int[] dropSum = new int[DropStatus.values().length];
        private float dropCount;
        private float refreshRate;
        private float fps;
        private long beginMs;

        public IActivityFrameListener listener;
        private int count = 0;

        ActivityFrameCollectItem(IActivityFrameListener listener) {
            this.listener = listener;
        }

        public void append(Activity activity, FrameMetrics frameMetrics, float droppedFrames, float refreshRate) {
            if ((listener.skipFirstFrame() && frameMetrics.getMetric(FrameMetrics.FIRST_DRAW_FRAME) == 1)
                    || droppedFrames < (refreshRate / 60) * listener.getThreshold()) {
                return;
            }
            if (count == 0) {
                beginMs = SystemClock.uptimeMillis();
            }
            for (int i = FrameDuration.UNKNOWN_DELAY_DURATION.ordinal(); i <= FrameDuration.TOTAL_DURATION.ordinal(); i++) {
                durations[i] += frameMetrics.getMetric(i);
            }
            if (sdkInt >= Build.VERSION_CODES.S) {
                durations[FrameDuration.GPU_DURATION.ordinal()] += frameMetrics.getMetric(FrameMetrics.GPU_DURATION);
            }

            dropCount += droppedFrames;
            collect((int) droppedFrames);
            this.refreshRate += refreshRate;
            this.fps += Math.min(refreshRate, 1f * Constants.TIME_SECOND_TO_NANO / frameMetrics.getMetric(FrameMetrics.TOTAL_DURATION));
            ++count;

            if (SystemClock.uptimeMillis() - beginMs >= listener.getIntervalMs()) {
                dropCount /= count;
                this.refreshRate /= count;
                this.fps /= count;
                for (int i = 0; i < durations.length; i++) {
                    durations[i] /= count;
                }
                listener.onFrameMetricsAvailable(activity.getClass().getName(), durations, dropLevel, dropSum, dropCount, this.refreshRate, this.fps);

                reset();
            }
        }

        void collect(int droppedFrames) {
            if (droppedFrames >= frozenThreshold) {
                dropLevel[DropStatus.DROPPED_FROZEN.ordinal()]++;
                dropSum[DropStatus.DROPPED_FROZEN.ordinal()] += droppedFrames;
            } else if (droppedFrames >= highThreshold) {
                dropLevel[DropStatus.DROPPED_HIGH.ordinal()]++;
                dropSum[DropStatus.DROPPED_HIGH.ordinal()] += droppedFrames;
            } else if (droppedFrames >= middleThreshold) {
                dropLevel[DropStatus.DROPPED_MIDDLE.ordinal()]++;
                dropSum[DropStatus.DROPPED_MIDDLE.ordinal()] += droppedFrames;
            } else if (droppedFrames >= normalThreshold) {
                dropLevel[DropStatus.DROPPED_NORMAL.ordinal()]++;
                dropSum[DropStatus.DROPPED_NORMAL.ordinal()] += droppedFrames;
            } else {
                dropLevel[DropStatus.DROPPED_BEST.ordinal()]++;
                dropSum[DropStatus.DROPPED_BEST.ordinal()] += Math.max(droppedFrames, 0);
            }
        }

        private void reset() {
            dropCount = 0;
            refreshRate = 0;
            fps = 0;
            count = 0;

            Arrays.fill(durations, 0);
            Arrays.fill(dropLevel, 0);
            Arrays.fill(dropSum, 0);
        }
    }

    /**
     * This method is reserved for compatibility. Using {@code setDropFrameListener} method
     * as alternative for api level greater equal N(24).
     */
    @Deprecated
    public void addDropFrameListener(int dropFrameListenerThreshold, DropFrameListener dropFrameListener) {
        this.oldDropFrameListener = dropFrameListener;
        this.dropFrameListenerThreshold = dropFrameListenerThreshold;
    }

    public void setDropFrameListener(int dropFrameListenerThreshold, IDropFrameListener dropFrameListener) {
        this.dropFrameListener = dropFrameListener;
        this.dropFrameListenerThreshold = dropFrameListenerThreshold;
    }

    public void removeDropFrameListener() {
        this.oldDropFrameListener = null;
        this.dropFrameListener = null;
    }

    @Deprecated
    public interface DropFrameListener {
        void dropFrame(int droppedFrame, long jitter, String scene);
    }

    @RequiresApi(Build.VERSION_CODES.N)
    public static String metricsToString(FrameMetrics frameMetrics) {
        StringBuilder sb = new StringBuilder();

        sb.append("{unknown_delay_duration=").append(frameMetrics.getMetric(FrameMetrics.UNKNOWN_DELAY_DURATION));
        sb.append(", input_handling_duration=").append(frameMetrics.getMetric(FrameMetrics.INPUT_HANDLING_DURATION));
        sb.append(", animation_duration=").append(frameMetrics.getMetric(FrameMetrics.ANIMATION_DURATION));
        sb.append(", layout_measure_duration=").append(frameMetrics.getMetric(FrameMetrics.LAYOUT_MEASURE_DURATION));
        sb.append(", draw_duration=").append(frameMetrics.getMetric(FrameMetrics.DRAW_DURATION));
        sb.append(", sync_duration=").append(frameMetrics.getMetric(FrameMetrics.SYNC_DURATION));
        sb.append(", command_issue_duration=").append(frameMetrics.getMetric(FrameMetrics.COMMAND_ISSUE_DURATION));
        sb.append(", swap_buffers_duration=").append(frameMetrics.getMetric(FrameMetrics.SWAP_BUFFERS_DURATION));
        sb.append(", total_duration=").append(frameMetrics.getMetric(FrameMetrics.TOTAL_DURATION));
        sb.append(", first_draw_frame=").append(frameMetrics.getMetric(FrameMetrics.FIRST_DRAW_FRAME));
        if (FrameTracer.sdkInt >= Build.VERSION_CODES.S) {
            sb.append(", gpu_duration=").append(frameMetrics.getMetric(FrameMetrics.GPU_DURATION));
        }
        sb.append("}");

        return sb.toString();
    }

    @Override
    public void onActivityCreated(final Activity activity, Bundle savedInstanceState) {

    }

    @Override
    public void onActivityStarted(Activity activity) {

    }

    private float getRefreshRate(Activity activity) {
        if (sdkInt >= Build.VERSION_CODES.R) {
            return activity.getDisplay().getRefreshRate();
        }
        return activity.getWindowManager().getDefaultDisplay().getRefreshRate();
    }

    @RequiresApi(Build.VERSION_CODES.N)
    @Override
    public void onActivityResumed(final Activity activity) {
        if (frameListenerMap.containsKey(activity.hashCode())) {
            return;
        }

        defaultRefreshRate = getRefreshRate(activity);
        MatrixLog.i(TAG, "default refresh rate is %dHz", (int) defaultRefreshRate);
        Window.OnFrameMetricsAvailableListener onFrameMetricsAvailableListener = new Window.OnFrameMetricsAvailableListener() {
            @RequiresApi(api = Build.VERSION_CODES.O)
            @Override
            public void onFrameMetricsAvailable(Window window, FrameMetrics frameMetrics, int dropCountSinceLastInvocation) {
                if (isForeground()) {
                    FrameMetrics frameMetricsCopy = new FrameMetrics(frameMetrics);
                    float refreshRate = getRefreshRate(activity);
                    long totalDuration = frameMetricsCopy.getMetric(FrameMetrics.TOTAL_DURATION);
                    float frameIntervalNanos = Constants.TIME_SECOND_TO_NANO / refreshRate;
                    long jitter = (long) (totalDuration - frameIntervalNanos);
                    int droppedFrames = Math.max(0, (int) Math.ceil(jitter / frameIntervalNanos));

                    droppedSum += droppedFrames;

                    if (dropFrameListener != null && droppedFrames >= dropFrameListenerThreshold) {
                        dropFrameListener.onFrameMetricsAvailable(activity, frameMetricsCopy, droppedFrames, refreshRate);
                    }
                    synchronized (listeners) {
                        for (IFrameListener observer : listeners) {
                            observer.onFrameMetricsAvailable(activity, frameMetricsCopy, droppedFrames, refreshRate);
                        }
                    }
                }
            }
        };

        this.frameListenerMap.put(activity.hashCode(), onFrameMetricsAvailableListener);
        activity.getWindow().addOnFrameMetricsAvailableListener(onFrameMetricsAvailableListener, MatrixHandlerThread.getDefaultHandler());
        MatrixLog.i(TAG, "onActivityResumed addOnFrameMetricsAvailableListener");
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

    @RequiresApi(Build.VERSION_CODES.N)
    @Override
    public void onActivityDestroyed(Activity activity) {
        try {
            activity.getWindow().removeOnFrameMetricsAvailableListener(frameListenerMap.remove(activity.hashCode()));
        } catch (Throwable t) {
            MatrixLog.e(TAG, "removeOnFrameMetricsAvailableListener error : " + t.getMessage());
        }
    }

    @RequiresApi(Build.VERSION_CODES.N)
    static class AllActivityFrameListener implements IActivityFrameListener {
        private static final String TAG = "AllActivityFrameListener";

        @Override
        public int getIntervalMs() {
            return Constants.DEFAULT_FPS_TIME_SLICE_ALIVE_MS;
        }

        @Override
        public String getName() {
            return null;
        }

        @Override
        public boolean skipFirstFrame() {
            return false;
        }

        @Override
        public int getThreshold() {
            return 0;
        }

        @Override
        public void onFrameMetricsAvailable(@NonNull String scene, long[] avgDurations, int[] dropLevel, int[] dropSum, float avgDroppedFrame, float avgRefreshRate, float fps) {
            MatrixLog.i(TAG, "[report] FPS:%s %s", fps, toString());
            try {
                TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
                if (null == plugin) {
                    return;
                }
                JSONObject dropLevelObject = new JSONObject();
                JSONObject dropSumObject = new JSONObject();
                for (DropStatus dropStatus : DropStatus.values()) {
                    dropLevelObject.put(dropStatus.name(), dropLevel[dropStatus.ordinal()]);
                    dropSumObject.put(dropStatus.name(), dropSum[dropStatus.ordinal()]);
                }

                JSONObject resultObject = new JSONObject();
                DeviceUtil.getDeviceInfo(resultObject, plugin.getApplication());

                resultObject.put(SharePluginInfo.ISSUE_SCENE, scene);
                resultObject.put(SharePluginInfo.ISSUE_DROP_LEVEL, dropLevelObject);
                resultObject.put(SharePluginInfo.ISSUE_DROP_SUM, dropSumObject);
                resultObject.put(SharePluginInfo.ISSUE_FPS, fps);

                for (FrameDuration frameDuration : FrameDuration.values()) {
                    resultObject.put(frameDuration.name(), avgDurations[frameDuration.ordinal()]);
                    if (frameDuration.equals(FrameDuration.TOTAL_DURATION)) {
                        break;
                    }
                }
                if (sdkInt >= Build.VERSION_CODES.S) {
                    resultObject.put("GPU_DURATION", avgDurations[FrameDuration.GPU_DURATION.ordinal()]);
                }
                resultObject.put("DROP_COUNT", Math.round(avgDroppedFrame));
                resultObject.put("REFRESH_RATE", (int) (avgRefreshRate));

                Issue issue = new Issue();
                issue.setTag(SharePluginInfo.TAG_PLUGIN_FPS);
                issue.setContent(resultObject);
                plugin.onDetectIssue(issue);

            } catch (JSONException e) {
                MatrixLog.e(TAG, "json error", e);
            }
        }
    }
}