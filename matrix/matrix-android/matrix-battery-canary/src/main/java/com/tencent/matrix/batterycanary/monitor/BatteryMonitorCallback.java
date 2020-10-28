package com.tencent.matrix.batterycanary.monitor;

import android.os.HandlerThread;
import android.os.Process;
import android.support.annotation.NonNull;
import android.util.LongSparseArray;

import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.LooperTaskMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;
import com.tencent.matrix.util.MatrixLog;

import java.util.List;

/**
 * @author Kaede
 * @since 2020/10/27
 */
public interface BatteryMonitorCallback extends JiffiesMonitorFeature.JiffiesListener, LooperTaskMonitorFeature.LooperTaskListener, WakeLockMonitorFeature.WakeLockListener {

    @SuppressWarnings("NotNullFieldNotInitialized")
    class BatteryPrinter implements BatteryMonitorCallback {
        private static final String TAG = "Matrix.BatteryPrinter";
        private static final int ONE_MIN = 60 * 1000;

        @NonNull
        private BatteryMonitorCore mMonitorCore;
        private final LongSparseArray<List<LooperTaskMonitorFeature.TaskTraceInfo>> tasks = new LongSparseArray<>();
        private WakeLockMonitorFeature.Info lastWakeInfo = null;
        private int diffWakeCount = 0;
        private long diffWakeTime = 0L;

        final BatteryPrinter attach(BatteryMonitorCore monitorCore) {
            mMonitorCore = monitorCore;
            return this;
        }

        @Override
        public void onTraceBegin() {
            WakeLockMonitorFeature plugin = mMonitorCore.getMonitorFeature(WakeLockMonitorFeature.class);
            if (null != plugin) {
                lastWakeInfo = plugin.getInfo();
            }
        }

        @Override
        public void onTraceEnd() {
            WakeLockMonitorFeature plugin = mMonitorCore.getMonitorFeature(WakeLockMonitorFeature.class);
            if (null != plugin && null != lastWakeInfo) {
                WakeLockMonitorFeature.Info info = plugin.getInfo();
                diffWakeCount = info.wakeLockCount - lastWakeInfo.wakeLockCount;
                diffWakeTime = info.wakeLockTime - lastWakeInfo.wakeLockTime;
            }
        }

        @Override
        public void onJiffies(JiffiesMonitorFeature.JiffiesResult result) {
            StringBuilder sb = new StringBuilder("\t\n");
            sb.append("****************************************** PowerTest *****************************************").append("\n");
            sb.append("| ").append("pid=").append(Process.myPid())
                    .append("\t\t").append("isForeground=").append(result.isForeground)
                    .append("\t\t").append("during(min)=").append(result.upTimeDiff / ONE_MIN).append("<").append(result.timeDiff / ONE_MIN)
                    .append("\t\t").append("diff(jiffies)=").append(result.jiffiesDiff2)
                    .append("\t\t").append("average(jiffies/min)=").append(result.jiffiesDiff2 / Math.max(1, result.upTimeDiff / ONE_MIN)).append("\n");
            sb.append("==============================================================================================").append("\n");
            for (JiffiesMonitorFeature.ThreadResult threadResult : result.threadResults.subList(0, Math.min(result.threadResults.size(), 8))) {
                if (threadResult.jiffiesDiff <= 0) {
                    sb.append("|\t\t......\n");
                    break;
                }
                sb.append("| -> ").append(threadResult).append("\n");
                List<LooperTaskMonitorFeature.TaskTraceInfo> threadTasks = tasks.get(threadResult.threadInfo.tid);
                if (null != threadTasks && !threadTasks.isEmpty()) {
                    for (LooperTaskMonitorFeature.TaskTraceInfo task : threadTasks.subList(0, Math.min(3, threadTasks.size()))) {
                        sb.append("|\t\t").append(task).append("\n");
                    }
                }
            }
            sb.append("| -> ").append("incrementWakeCount=").append(diffWakeCount).append(" sumWakeTime=").append(diffWakeTime).append("ms\n");
            sb.append(getExtInfo());
            sb.append("**********************************************************************************************");
            MatrixLog.i(TAG, "%s", sb.toString());
            synchronized (tasks) {
                tasks.clear();
            }
        }

        public StringBuilder getExtInfo() {
            return new StringBuilder();
        }

        @Override
        public void onTaskTrace(Thread thread, List<LooperTaskMonitorFeature.TaskTraceInfo> sortList) {
            if (thread instanceof HandlerThread) {
                synchronized (tasks) {
                    tasks.put(((HandlerThread) thread).getThreadId(), sortList);
                }
            }
        }

        @Override
        public void onWakeLockTimeout(String tag, String packageName, int warningCount) {

        }
    }
}
