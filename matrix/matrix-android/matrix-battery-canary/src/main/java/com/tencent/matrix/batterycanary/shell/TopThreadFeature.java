package com.tencent.matrix.batterycanary.shell;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback.BatteryPrinter.Printer;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore.Callback;
import com.tencent.matrix.batterycanary.monitor.feature.AbsMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;

import androidx.annotation.Nullable;

/**
 * Something like 'adb shell top -Hb -u <uid>'
 *
 * @author Kaede
 * @since 2022/2/22
 */
public class TopThreadFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.TopThread";

    @Nullable private JiffiesSnapshot mLastSnapshot;
    @Nullable private Runnable mTopTask;

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public int weight() {
        return Integer.MIN_VALUE;
    }

    public void schedule(final int seconds) {
        final JiffiesMonitorFeature jiffiesFeat = mCore.getMonitorFeature(JiffiesMonitorFeature.class);
        if (jiffiesFeat != null) {
            if (mTopTask == null) {
                mTopTask = new Runnable() {
                    @Override
                    public void run() {
                        jiffiesFeat.currentJiffiesSnapshot(new Callback<JiffiesSnapshot>() {
                            @Override
                            public void onGetJiffies(JiffiesSnapshot snapshot) {
                                if (mLastSnapshot != null) {
                                    Delta<JiffiesSnapshot> delta = snapshot.diff(mLastSnapshot);
                                    if (delta.isValid()) {
                                        Printer printer = new Printer();
                                        printer.writeTitle();
                                        printer.writeTitle();
                                        printer.append("| TOP Thread").append("\n");

                                        printer.createSection("Proc");
                                        printer.writeLine("pid", String.valueOf(delta.dlt.pid));
                                        printer.writeLine("cmm", String.valueOf(delta.dlt.name));

                                        printer.createSection("Thread(" + delta.dlt.threadEntries.getList().size() + ")");
                                        printer.writeLine("TID \tLOAD \t(STATUS)THREAD_NAME \tJIFFY");
                                        for (JiffiesSnapshot.ThreadJiffiesEntry threadJiffies : delta.dlt.threadEntries.getList()) {
                                            long entryJffies = threadJiffies.get();
                                            printer.append("|   -> ")
                                                    .append(threadJiffies.tid).append("\t")
                                                    .append((int) ((entryJffies / (seconds * 100f)) * 100)).append("%\t")
                                                    .append("(").append(threadJiffies.isNewAdded ? "+" : "~").append("/").append(threadJiffies.stat).append(")").append(threadJiffies.name).append("\t")
                                                    .append(entryJffies).append("\t")
                                                    .append("\n");
                                        }

                                        printer.writeEnding();
                                        printer.dump();
                                    }
                                }
                                mLastSnapshot = snapshot;
                                mCore.getHandler().postDelayed(mTopTask, seconds * 1000L);
                            }
                        });
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
        mLastSnapshot = null;
    }
}
