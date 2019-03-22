package com.tencent.matrix.trace.tracer;

import android.os.Handler;

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

public class FrameTracer extends Tracer implements UIThreadMonitor.IFrameObserver {

    private static final String TAG = "Matrix.FrameTracer";
    private HashSet<IDoFrameListener> listeners = new HashSet<>();
    private final long frameIntervalNanos;
    private final TraceConfig config;

    public FrameTracer(TraceConfig config) {
        this.config = config;
        this.frameIntervalNanos = UIThreadMonitor.getMonitor().getFrameIntervalNanos();
        if (config.isFPSEnable()) {
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
            listeners.add(listener);
        }
    }

    @Override
    public void onAlive() {
        UIThreadMonitor.getMonitor().addObserver(this);
    }

    @Override
    public void onDead() {
        UIThreadMonitor.getMonitor().removeObserver(this);
    }


    @Override
    public void doFrame(String focusedActivityName, long start, long end, long frameCostMs, long inputCostNs, long animationCostNs, long traversalCostNs) {

        notifyListener(focusedActivityName, frameCostMs);
    }

    private void notifyListener(final String focusedActivityName, final long frameCostMs) {
        long start = System.currentTimeMillis();
        try {
            synchronized (listeners) {
                for (final IDoFrameListener listener : listeners) {
                    final int dropFrame = (int) (frameCostMs / (frameIntervalNanos / Constants.TIME_MILLIS_TO_NANO));
                    listener.doFrameSync(focusedActivityName, frameCostMs, dropFrame);
                    if (null != listener.getHandler()) {
                        listener.getHandler().post(new Runnable() {
                            @Override
                            public void run() {
                                listener.doFrameAsync(focusedActivityName, frameCostMs, dropFrame);
                            }
                        });
                    }
                }
            }
        } finally {
            long cost = System.currentTimeMillis() - start;
            if (config.isDevEnv()) {
                MatrixLog.v(TAG, "[notifyListener] cost:%sms", cost);
            }
            if (cost > (frameIntervalNanos / Constants.TIME_MILLIS_TO_NANO)) {
                MatrixLog.w(TAG, "[notifyListener] warm! maybe do heavy work in doFrameSync,but you can replace with doFrameAsync! cost:%sms", cost);
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
        public void doFrameAsync(String focusedActivityName, long frameCost, int droppedFrames) {
            super.doFrameAsync(focusedActivityName, frameCost, droppedFrames);
            if (Utils.isEmpty(focusedActivityName)) {
                return;
            }

            FrameCollectItem item = map.get(focusedActivityName);
            if (null == item) {
                item = new FrameCollectItem(focusedActivityName);
                map.put(focusedActivityName, item);
            }

            item.collect(droppedFrames);

            if (item.sumFrameCost >= config.getTimeSliceMs()) { // report
                map.remove(focusedActivityName);
                item.report();
            }
        }

    }

    private class FrameCollectItem {
        String focusedActivityName;
        long sumFrameCost;
        int sumFrame = 0;
        int sumDroppedFrames;
        // record the level of frames dropped each time
        int[] dropLevel = new int[DropStatus.values().length];
        int[] dropSum = new int[DropStatus.values().length];

        FrameCollectItem(String focusedActivityName) {
            this.focusedActivityName = focusedActivityName;
        }

        void collect(int droppedFrames) {
            long frameIntervalCost = UIThreadMonitor.getMonitor().getFrameIntervalNanos();
            sumFrameCost += (droppedFrames + 1) * frameIntervalCost / Constants.TIME_MILLIS_TO_NANO;
            sumDroppedFrames += droppedFrames;
            sumFrame++;

            if (droppedFrames >= config.getFrozenThreshold()) {
                dropLevel[DropStatus.DROPPED_FROZEN.index]++;
                dropSum[DropStatus.DROPPED_FROZEN.index] += droppedFrames;
            } else if (droppedFrames >= config.getHighThreshold()) {
                dropLevel[DropStatus.DROPPED_HIGH.index]++;
                dropSum[DropStatus.DROPPED_HIGH.index] += droppedFrames;
            } else if (droppedFrames >= config.getMiddleThreshold()) {
                dropLevel[DropStatus.DROPPED_MIDDLE.index]++;
                dropSum[DropStatus.DROPPED_MIDDLE.index] += droppedFrames;
            } else if (droppedFrames >= config.getNormalThreshold()) {
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

                resultObject.put(SharePluginInfo.ISSUE_SCENE, focusedActivityName);
                resultObject.put(SharePluginInfo.ISSUE_DROP_LEVEL, dropLevelObject);
                resultObject.put(SharePluginInfo.ISSUE_DROP_SUM, dropSumObject);
                resultObject.put(SharePluginInfo.ISSUE_FPS, fps);

                Issue issue = new Issue();
                issue.setTag(SharePluginInfo.TAG_PLUGIN_FPS);
                issue.setContent(resultObject);
                plugin.onDetectIssue(issue);

            } catch (JSONException e) {
                MatrixLog.e(TAG, "json error", e);
            }
        }


        @Override
        public String toString() {
            return "focusedActivityName=" + focusedActivityName
                    + ", sumFrame=" + sumFrame
                    + ", sumDroppedFrames=" + sumDroppedFrames
                    + ", sumFrameCost=" + sumFrameCost
                    + ", dropLevel=" + Arrays.toString(dropLevel);
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
