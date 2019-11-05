package com.tencent.matrix.trace.tracer;

import android.os.Handler;
import android.os.SystemClock;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.SharePluginInfo;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.core.UIThreadMonitor;
import com.tencent.matrix.trace.listeners.IDoFrameListener;
import com.tencent.matrix.trace.util.Utils;
import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.concurrent.TimeUnit;

public class FrameTracer extends Tracer {

    private static final String TAG = "Matrix.FrameTracer";
    private HashSet<IDoFrameListener> listeners = new HashSet<>();
    private final long frameIntervalMs;
    private final TraceConfig config;
    private long timeSliceMs;
    private boolean isFPSEnable;
    private long frozenThreshold;
    private long highThreshold;
    private long middleThreshold;
    private long normalThreshold;

    public FrameTracer(TraceConfig config) {
        this.config = config;
        this.frameIntervalMs = TimeUnit.MILLISECONDS.convert(UIThreadMonitor.getMonitor().getFrameIntervalNanos(), TimeUnit.NANOSECONDS) + 1;
        this.timeSliceMs = config.getTimeSliceMs();
        this.isFPSEnable = config.isFPSEnable();
        this.frozenThreshold = config.getFrozenThreshold();
        this.highThreshold = config.getHighThreshold();
        this.normalThreshold = config.getNormalThreshold();
        this.middleThreshold = config.getMiddleThreshold();

        MatrixLog.i(TAG, "[init] frameIntervalMs:%s isFPSEnable:%s", frameIntervalMs, isFPSEnable);
        if (isFPSEnable) {
            addListener(new FPSCollector());
        }
    }

    public void addListener(IDoFrameListener listener) {
        synchronized (listeners) {
            listeners.add(listener);
        }
    }

    public void removeListener(IDoFrameListener listener) {
        synchronized (listeners) {
            listeners.remove(listener);
        }
    }

    @Override
    public void onAlive() {
        super.onAlive();
        UIThreadMonitor.getMonitor().addObserver(this);
    }

    @Override
    public void onDead() {
        super.onDead();
        UIThreadMonitor.getMonitor().removeObserver(this);
    }


    @Override
    public void doFrame(String focusedActivityName, long start, long end, long frameCostMs, long inputCostNs, long animationCostNs, long traversalCostNs) {
        if (isForeground()) {
            notifyListener(focusedActivityName, frameCostMs);
        }
    }


    private void notifyListener(final String visibleScene, final long frameCostMs) {
        long start = System.currentTimeMillis();
        try {
            synchronized (listeners) {
                for (final IDoFrameListener listener : listeners) {
                    if (config.isDevEnv()) {
                        listener.time = SystemClock.uptimeMillis();
                    }
                    final int dropFrame = (int) (frameCostMs / frameIntervalMs);
                    listener.doFrameSync(visibleScene, frameCostMs, dropFrame);
                    if (null != listener.getHandler()) {
                        listener.getHandler().post(new Runnable() {
                            @Override
                            public void run() {
                                listener.doFrameAsync(visibleScene, frameCostMs, dropFrame);
                            }
                        });
                    }
                    if (config.isDevEnv()) {
                        listener.time = SystemClock.uptimeMillis() - listener.time;
                        MatrixLog.d(TAG, "[notifyListener] cost:%sms listener:%s", listener.time, listener);
                    }
                }
            }
        } finally {
            long cost = System.currentTimeMillis() - start;
            if (config.isDebug() && cost > frameIntervalMs) {
                MatrixLog.w(TAG, "[notifyListener] warm! maybe do heavy work in doFrameSync! size:%s cost:%sms", listeners.size(), cost);
            }
        }
    }


    private class FPSCollector extends IDoFrameListener {

        private Handler frameHandler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper());
        private HashMap<String, FrameCollectItem> map = new HashMap<>();

        @Override
        public Handler getHandler() {
            return frameHandler;
        }

        @Override
        public void doFrameAsync(String visibleScene, long frameCost, int droppedFrames) {
            super.doFrameAsync(visibleScene, frameCost, droppedFrames);
            if (Utils.isEmpty(visibleScene)) {
                return;
            }

            FrameCollectItem item = map.get(visibleScene);
            if (null == item) {
                item = new FrameCollectItem(visibleScene);
                map.put(visibleScene, item);
            }

            item.collect(droppedFrames);

            if (item.sumFrameCost >= timeSliceMs) { // report
                map.remove(visibleScene);
                item.report();
            }
        }

    }

    private class FrameCollectItem {
        String visibleScene;
        long sumFrameCost;
        int sumFrame = 0;
        int sumDroppedFrames;
        // record the level of frames dropped each time
        int[] dropLevel = new int[DropStatus.values().length];
        int[] dropSum = new int[DropStatus.values().length];

        FrameCollectItem(String visibleScene) {
            this.visibleScene = visibleScene;
        }

        void collect(int droppedFrames) {
            long frameIntervalCost = UIThreadMonitor.getMonitor().getFrameIntervalNanos();
            sumFrameCost += (droppedFrames + 1) * frameIntervalCost / Constants.TIME_MILLIS_TO_NANO;
            sumDroppedFrames += droppedFrames;
            sumFrame++;

            if (droppedFrames >= frozenThreshold) {
                dropLevel[DropStatus.DROPPED_FROZEN.index]++;
                dropSum[DropStatus.DROPPED_FROZEN.index] += droppedFrames;
            } else if (droppedFrames >= highThreshold) {
                dropLevel[DropStatus.DROPPED_HIGH.index]++;
                dropSum[DropStatus.DROPPED_HIGH.index] += droppedFrames;
            } else if (droppedFrames >= middleThreshold) {
                dropLevel[DropStatus.DROPPED_MIDDLE.index]++;
                dropSum[DropStatus.DROPPED_MIDDLE.index] += droppedFrames;
            } else if (droppedFrames >= normalThreshold) {
                dropLevel[DropStatus.DROPPED_NORMAL.index]++;
                dropSum[DropStatus.DROPPED_NORMAL.index] += droppedFrames;
            } else {
                dropLevel[DropStatus.DROPPED_BEST.index]++;
                dropSum[DropStatus.DROPPED_BEST.index] += (droppedFrames < 0 ? 0 : droppedFrames);
            }
        }

        void report() {
            float fps = Math.min(60.f, 1000.f * sumFrame / sumFrameCost);
            MatrixLog.i(TAG, "[report] FPS:%s %s", fps, toString());

            try {
                TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
                if (null == plugin) {
                    return;
                }
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
                resultObject = DeviceUtil.getDeviceInfo(resultObject, plugin.getApplication());

                resultObject.put(SharePluginInfo.ISSUE_SCENE, visibleScene);
                resultObject.put(SharePluginInfo.ISSUE_DROP_LEVEL, dropLevelObject);
                resultObject.put(SharePluginInfo.ISSUE_DROP_SUM, dropSumObject);
                resultObject.put(SharePluginInfo.ISSUE_FPS, fps);

                Issue issue = new Issue();
                issue.setTag(SharePluginInfo.TAG_PLUGIN_FPS);
                issue.setContent(resultObject);
                plugin.onDetectIssue(issue);

            } catch (JSONException e) {
                MatrixLog.e(TAG, "json error", e);
            } finally {
                sumFrame = 0;
                sumDroppedFrames = 0;
                sumFrameCost = 0;
            }
        }


        @Override
        public String toString() {
            return "visibleScene=" + visibleScene
                    + ", sumFrame=" + sumFrame
                    + ", sumDroppedFrames=" + sumDroppedFrames
                    + ", sumFrameCost=" + sumFrameCost
                    + ", dropLevel=" + Arrays.toString(dropLevel);
        }
    }

    public enum DropStatus {
        DROPPED_FROZEN(4), DROPPED_HIGH(3), DROPPED_MIDDLE(2), DROPPED_NORMAL(1), DROPPED_BEST(0);
        int index;

        DropStatus(int index) {
            this.index = index;
        }

    }
}
