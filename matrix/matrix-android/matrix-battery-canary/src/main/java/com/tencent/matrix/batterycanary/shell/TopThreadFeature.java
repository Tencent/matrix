package com.tencent.matrix.batterycanary.shell;

import android.app.ActivityManager;
import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Process;
import android.util.SparseArray;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback.BatteryPrinter.Printer;
import com.tencent.matrix.batterycanary.monitor.feature.AbsMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.CompositeMonitors;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

import androidx.annotation.Nullable;
import androidx.core.util.Pair;
import androidx.core.util.Supplier;

/**
 * Something like 'adb shell top -Hb -u <uid>'
 *
 * @author Kaede
 * @since 2022/2/22
 */
public class TopThreadFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.TopThread";
    private boolean sStopShell;

    public interface ContinuousCallback {
        boolean onGetDeltas(CompositeMonitors monitors, long windowMillis);
    }

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

    public void topShell(final int seconds) {
        sStopShell = false;
        top(seconds, new Supplier<CompositeMonitors>() {
            @Override
            public CompositeMonitors get() {
                CompositeMonitors monitors = new CompositeMonitors(mCore, CompositeMonitors.SCOPE_TOP_SHELL);
                monitors.metric(JiffiesMonitorFeature.UidJiffiesSnapshot.class);
                return monitors;
            }
        }, new ContinuousCallback() {
            @Override
            public boolean onGetDeltas(CompositeMonitors monitors, long windowMillis) {
                // Proc Load
                List<Delta<JiffiesSnapshot>> deltaList = monitors.getAllPidDeltaList();
                long allProcJiffies = 0;
                for (Delta<JiffiesSnapshot> delta : deltaList) {
                    allProcJiffies += delta.dlt.totalJiffies.get();
                }
                float totalLoad = figureCupLoad(allProcJiffies, seconds * 100L);

                Printer printer = new Printer();
                printer.writeTitle();
                printer.append("| TOP Thread\tpidNum=").append(deltaList.size())
                        .append("\tcpuLoad=").append(formatFloat(totalLoad, 1)).append("%")
                        .append("\n");

                // Thread Load
                for (Delta<JiffiesSnapshot> delta : deltaList) {
                    if (delta.isValid()) {
                        printer.createSection("Proc");
                        printer.writeLine("pid", String.valueOf(delta.dlt.pid));
                        printer.writeLine("cmm", String.valueOf(delta.dlt.name));
                        printer.writeLine("load", formatFloat(figureCupLoad(delta.dlt.totalJiffies.get(), seconds * 100L), 1) + "%");
                        printer.createSubSection("Thread(" + delta.dlt.threadEntries.getList().size() + ")");
                        printer.writeLine("  TID\tLOAD \tSTATUS \tTHREAD_NAME \tJIFFY");
                        for (JiffiesSnapshot.ThreadJiffiesEntry threadJiffies : delta.dlt.threadEntries.getList()) {
                            long entryJffies = threadJiffies.get();
                            printer.append("|   -> ")
                                    .append(fixedColumn(String.valueOf(threadJiffies.tid), 5)).append("\t")
                                    .append(fixedColumn(formatFloat(figureCupLoad(entryJffies, seconds * 100L), 1), 4)).append("\t")
                                    .append(threadJiffies.isNewAdded ? "+" : "~").append("/").append(threadJiffies.stat).append("\t")
                                    .append(fixedColumn(threadJiffies.name, 16)).append("\t")
                                    .append(entryJffies).append("\t")
                                    .append("\n");
                        }
                    }
                }

                printer.writeEnding();
                printer.dump();
                return sStopShell;
            }
        });
    }

    public void stopShell() {
        sStopShell = true;
        if (mTopTask != null) {
            mCore.getHandler().removeCallbacks(mTopTask);
            mTopTask = null;
        }
        mLastPidJiffiesHolder.clear();
    }


    public void top(final int seconds, final Supplier<CompositeMonitors> supplier, final ContinuousCallback callback) {
        final JiffiesMonitorFeature jiffiesFeat = mCore.getMonitorFeature(JiffiesMonitorFeature.class);
        if (jiffiesFeat == null) {
            return;
        }
        final long windowMillis = seconds * 1000L;
        final AtomicReference<CompositeMonitors> lastMonitors = new AtomicReference<>(null);
        HandlerThread thread = new HandlerThread("matrix_top");
        thread.start();
        final Handler handler = new Handler(thread.getLooper());
        final Runnable action = new Runnable() {
            @Override
            public void run() {
                CompositeMonitors monitors = lastMonitors.get();
                if (monitors == null) {
                    // Fist time
                    scheduleNext();

                } else {
                    lastMonitors.set(null);
                    monitors.finish();
                    boolean stop = callback.onGetDeltas(monitors, windowMillis);
                    if (stop) {
                        handler.getLooper().quit();
                    } else {
                        // Next
                        scheduleNext();
                    }
                }
            }

            private void scheduleNext() {
                CompositeMonitors monitors = supplier.get();
                monitors.start();
                lastMonitors.set(monitors);
                handler.postDelayed(this, windowMillis);
            }
        };
        handler.postDelayed(action, windowMillis);
    }

    static List<Integer> getAllPidList(Context context) {
        ArrayList<Integer> list = new ArrayList<>();
        list.add(Process.myPid());
        List<Pair<Integer, String>> procList = getProcList(context);
        for (Pair<Integer, String> item : procList) {
            if (!list.contains(item.first)) {
                list.add(item.first);
            }
        }
        return list;
    }

    public static List<Pair<Integer, String>> getProcList(Context context) {
        ArrayList<Pair<Integer, String>> list = new ArrayList<>();
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        if (am != null) {
            List<ActivityManager.RunningAppProcessInfo> processes = am.getRunningAppProcesses();
            if (processes != null) {
                for (ActivityManager.RunningAppProcessInfo item : processes) {
                    if (item.processName.contains(context.getPackageName())) {
                        list.add(new Pair<>(item.pid, item.processName));
                    }
                }
            }
        }
        return list;
    }

    public static float figureCupLoad(long jiffies, long cpuJiffies) {
        return (jiffies / (cpuJiffies * 1f)) * 100;
    }

    public static String formatFloat(float input, int decimal) {
        return String.format("%." + decimal + "f", input);
    }

    public static String fixedColumn(String input, int width) {
        if (input != null && input.length() >= width) {
            return input;
        }
        return repeat(" ", width - (input == null ? 0 : input.length())) + input;
    }

    @SuppressWarnings("SameParameterValue")
    static String repeat(String symbol, int count) {
        return new String(new char[count]).replace("\0", symbol);
    }
}
