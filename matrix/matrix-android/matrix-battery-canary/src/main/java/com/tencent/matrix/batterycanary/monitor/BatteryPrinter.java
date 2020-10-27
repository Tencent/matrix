package com.tencent.matrix.batterycanary.monitor;

import android.os.HandlerThread;
import android.os.Process;
import android.util.LongSparseArray;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryMonitor;
import com.tencent.matrix.batterycanary.monitor.plugin.JiffiesMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.plugin.LooperTaskMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.plugin.WakeLockMonitorPlugin;
import com.tencent.matrix.util.MatrixLog;

import java.util.List;

public class BatteryPrinter implements BatteryMonitor.Printer {

    private static final String TAG = "Matrix.BatteryPrinter";
    private static final int ONE_MIN = 60 * 1000;
    private final LongSparseArray<List<LooperTaskMonitorPlugin.TaskTraceInfo>> tasks = new LongSparseArray<>();
    private WakeLockMonitorPlugin.Info lastWakeInfo = null;
    private int diffWakeCount = 0;
    private long diffWakeTime = 0L;

    @Override
    public void onTraceBegin() {
        BatteryMonitor monitor = Matrix.with().getPluginByClass(BatteryMonitor.class);
        if (null != monitor) {
            WakeLockMonitorPlugin plugin = monitor.getMonitorPlugin(WakeLockMonitorPlugin.class);
            if (null != plugin) {
                lastWakeInfo = plugin.getInfo();
            }
        }
    }

    @Override
    public void onTraceEnd() {
        BatteryMonitor monitor = Matrix.with().getPluginByClass(BatteryMonitor.class);
        if (null != monitor) {
            WakeLockMonitorPlugin plugin = monitor.getMonitorPlugin(WakeLockMonitorPlugin.class);
            if (null != plugin && null != lastWakeInfo) {
                WakeLockMonitorPlugin.Info info = plugin.getInfo();
                diffWakeCount = info.wakeLockCount - lastWakeInfo.wakeLockCount;
                diffWakeTime = info.wakeLockTime - lastWakeInfo.wakeLockTime;
            }
        }
    }

    @Override
    public void onJiffies(JiffiesMonitorPlugin.JiffiesResult result) {

        StringBuilder sb = new StringBuilder("\t\n");
        sb.append("****************************************** PowerTest *****************************************").append("\n");
        sb.append("| ").append("pid=").append(Process.myPid())
                .append("\t\t").append("isForeground=").append(result.isForeground)
                .append("\t\t").append("during(min)=").append(result.upTimeDiff / ONE_MIN).append("<").append(result.timeDiff / ONE_MIN)
                .append("\t\t").append("diff(jiffies)=").append(result.jiffiesDiff2)
                .append("\t\t").append("average(jiffies/min)=").append(result.jiffiesDiff2 / Math.max(1, result.upTimeDiff / ONE_MIN)).append("\n");
        sb.append("==============================================================================================").append("\n");
        for (JiffiesMonitorPlugin.ThreadResult threadResult : result.threadResults.subList(0, Math.min(result.threadResults.size(), 8))) {
            if (threadResult.jiffiesDiff <= 0) {
                sb.append("|\t\t......\n");
                break;
            }
            sb.append("| -> ").append(threadResult).append("\n");
            List<LooperTaskMonitorPlugin.TaskTraceInfo> threadTasks = tasks.get(threadResult.threadInfo.tid);
            if (null != threadTasks && !threadTasks.isEmpty()) {
                for (LooperTaskMonitorPlugin.TaskTraceInfo task : threadTasks.subList(0, Math.min(3, threadTasks.size()))) {
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
    public void onTaskTrace(Thread thread, List<LooperTaskMonitorPlugin.TaskTraceInfo> sortList) {
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
