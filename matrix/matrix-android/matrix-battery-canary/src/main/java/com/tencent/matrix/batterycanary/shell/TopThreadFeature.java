package com.tencent.matrix.batterycanary.shell;

import android.app.ActivityManager;
import android.content.Context;
import android.os.Process;
import android.util.SparseArray;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback.BatteryPrinter.Printer;
import com.tencent.matrix.batterycanary.monitor.feature.AbsMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;

import java.util.ArrayList;
import java.util.List;

import androidx.annotation.Nullable;

/**
 * Something like 'adb shell top -Hb -u <uid>'
 *
 * @author Kaede
 * @since 2022/2/22
 */
public class TopThreadFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.TopThread";

    @Nullable private Runnable mTopTask;
    private final SparseArray<JiffiesSnapshot> mLastPidJiffiesHolder = new SparseArray<>();

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public int weight() {
        return Integer.MIN_VALUE;
    }

    @SuppressWarnings("SpellCheckingInspection")
    public void schedule(final int seconds) {
        final JiffiesMonitorFeature jiffiesFeat = mCore.getMonitorFeature(JiffiesMonitorFeature.class);
        if (jiffiesFeat != null) {
            if (mTopTask == null) {
                mTopTask = new Runnable() {

                    @Override
                    public void run() {
                        List<Delta<JiffiesSnapshot>> deltaList = new ArrayList<>();
                        List<Integer> pidList = getAllPidList(mCore.getContext());
                        for (Integer pid : pidList) {
                            JiffiesSnapshot curr = jiffiesFeat.currentJiffiesSnapshot(pid);
                            JiffiesSnapshot last = mLastPidJiffiesHolder.get(pid);
                            if (last != null) {
                                Delta<JiffiesSnapshot> delta = curr.diff(last);
                                if (delta.isValid()) {
                                    deltaList.add(delta);
                                }
                            }
                            mLastPidJiffiesHolder.put(pid, curr);
                        }

                        // Proc Load
                        long allProcJiffies = 0;
                        for (Delta<JiffiesSnapshot> delta : deltaList) {
                            allProcJiffies += delta.dlt.totalJiffies.get();
                        }
                        int totalLoad = (int) figureCupLoad(allProcJiffies, seconds * 100L);

                        Printer printer = new Printer();
                        printer.writeTitle();
                        printer.writeTitle();
                        printer.append("| TOP Thread\tpidNum=").append(pidList.size())
                                .append("\tcpuLoad=").append(totalLoad).append("%")
                                .append("\n");

                        // Thread Load
                        for (Delta<JiffiesSnapshot> delta : deltaList) {
                            if (delta.isValid()) {
                                printer.createSection("Proc");
                                printer.writeLine("pid", String.valueOf(delta.dlt.pid));
                                printer.writeLine("cmm", String.valueOf(delta.dlt.name));
                                printer.writeLine("load", (int) figureCupLoad(delta.dlt.totalJiffies.get(), seconds * 100L) + "%");
                                printer.createSubSection("Thread(" + delta.dlt.threadEntries.getList().size() + ")");
                                printer.writeLine("  TID\tLOAD \tSTATUS \tTHREAD_NAME \tJIFFY");
                                for (JiffiesSnapshot.ThreadJiffiesEntry threadJiffies : delta.dlt.threadEntries.getList()) {
                                    long entryJffies = threadJiffies.get();
                                    printer.append("|   -> ")
                                            .append(fixedColumn(String.valueOf(threadJiffies.tid), 5)).append("\t")
                                            .append(fixedColumn((int) figureCupLoad(entryJffies, seconds * 100L) + "%", 4)).append("\t")
                                            .append(threadJiffies.isNewAdded ? "+" : "~").append("/").append(threadJiffies.stat).append("\t")
                                            .append(fixedColumn(threadJiffies.name, 16)).append("\t")
                                            .append(entryJffies).append("\t")
                                            .append("\n");
                                }
                            }
                        }

                        printer.writeEnding();
                        printer.dump();
                        mCore.getHandler().postDelayed(mTopTask, seconds * 1000L);
                    }

                    String fixedColumn(String input, int width) {
                        if (input != null && input.length() >= width) {
                            return input;
                        }
                        return repeat(" ", width - (input == null ? 0 : input.length())) + input;
                    }

                    public String repeat(String with, int count) {
                        return new String(new char[count]).replace("\0", with);
                    }
                };
            }
            mCore.getHandler().postDelayed(mTopTask, seconds * 1000L);
        }
    }

    public void stop() {
        if (mTopTask != null) {
            mCore.getHandler().removeCallbacks(mTopTask);
            mTopTask = null;
        }
        mLastPidJiffiesHolder.clear();
    }

    static List<Integer> getAllPidList(Context context) {
        ArrayList<Integer> list = new ArrayList<>();
        list.add(Process.myPid());
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        if (am != null) {
            List<ActivityManager.RunningAppProcessInfo> processes = am.getRunningAppProcesses();
            for (ActivityManager.RunningAppProcessInfo item : processes) {
                if (item.processName.contains(context.getPackageName()) && !list.contains(item.pid)) {
                    list.add(item.pid);
                }
            }
        }
        return list;
    }

    static float figureCupLoad(long jiffies, long cpuJiffies) {
        return (jiffies / (cpuJiffies * 1f)) * 100;
    }
}
