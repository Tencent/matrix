package com.tencent.matrix.batterycanary.stats;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Process;

import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.monitor.feature.AbsMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.CompositeMonitors;
import com.tencent.matrix.util.MatrixHandlerThread;

import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Set;

import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;

/**
 * @author Kaede
 * @since 2021/12/3
 */
public final class BatteryStatsFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.BatteryStats";
    private static final int DAY_LIMIT = 7;

    private HandlerThread mStatsThread;
    private Handler mStatsHandler;
    private BatteryRecorder mBatteryRecorder;
    private BatteryStats mBatteryStats;
    private boolean mStatsImmediately = false;

    @Override
    public int weight() {
        return 0;
    }

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public void onTurnOn() {
        super.onTurnOn();
        mBatteryRecorder = mCore.getConfig().batteryRecorder;
        mBatteryStats = mCore.getConfig().batteryStats;
        if (mBatteryRecorder != null) {
            mStatsThread = MatrixHandlerThread.getNewHandlerThread("matrix-stats", Thread.NORM_PRIORITY);
            mStatsHandler = new Handler(mStatsThread.getLooper());
            mStatsHandler.post(new Runnable() {
                @Override
                public void run() {
                    mBatteryRecorder.updateProc(BatteryRecorder.MMKVRecorder.getProcNameSuffix());

                    // Clean expired records if need
                    mBatteryRecorder.clean(DAY_LIMIT);
                }
            });
        }

        BatteryRecord.ProcStatRecord procStatRecord = new BatteryRecord.ProcStatRecord();
        procStatRecord.pid = Process.myPid();
        procStatRecord.procStat = BatteryRecord.ProcStatRecord.STAT_PROC_LAUNCH;
        writeRecord(procStatRecord);
    }

    @Override
    public void onForeground(boolean isForeground) {
        super.onForeground(isForeground);
        statsAppStat(isForeground ? AppStats.APP_STAT_FOREGROUND : AppStats.APP_STAT_BACKGROUND);
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        BatteryRecord.ProcStatRecord procStatRecord = new BatteryRecord.ProcStatRecord();
        procStatRecord.pid = Process.myPid();
        procStatRecord.procStat = BatteryRecord.ProcStatRecord.STAT_PROC_OFF;
        writeRecord(procStatRecord);

        if (mStatsHandler != null) {
            mStatsHandler.post(new Runnable() {
                @Override
                public void run() {
                    if (mStatsThread != null) {
                        mStatsThread.quit();
                    }
                }
            });
        }
    }

    @VisibleForTesting
    public void setStatsImmediately(boolean statsImmediately) {
        mStatsImmediately = statsImmediately;
    }

    public Set<String> getProcSet() {
        if (mBatteryRecorder != null) {
            return mBatteryRecorder.getProcSet();
        }
        return Collections.emptySet();
    }

    public void writeRecord(final BatteryRecord record) {
        if (mBatteryRecorder != null) {
            final String date = getDateString(0);
            if (mStatsImmediately) {
                writeRecord(date, record);
                return;
            }
            if (mStatsHandler != null) {
                mStatsHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        writeRecord(date, record);
                    }
                });
            }
        }
    }

    @WorkerThread
    public List<BatteryRecord> readRecords(int dayOffset, String proc) {
        if (mBatteryRecorder != null) {
            final String date = getDateString(dayOffset);
            return mBatteryRecorder.read(date, proc);
        }
        return Collections.emptyList();
    }

    @WorkerThread
    public BatteryRecords readBatteryRecords(int dayOffset, String proc) {
        BatteryRecords batteryRecords = new BatteryRecords();
        batteryRecords.date = getDateString(dayOffset);
        batteryRecords.records = readRecords(dayOffset, proc);
        return batteryRecords;
    }

    @WorkerThread
    void writeRecord(String date, BatteryRecord record) {
        if (mBatteryRecorder != null) {
            mBatteryRecorder.write(date, record);
        }
    }

    @WorkerThread
    List<BatteryRecord> readRecords(String date, String proc) {
        if (mBatteryRecorder != null) {
            return mBatteryRecorder.read(date, proc);
        }
        return Collections.emptyList();
    }

    @WorkerThread
    void cleanRecords(String date, String proc) {
        if (mBatteryRecorder != null) {
            mBatteryRecorder.clean(date, proc);
        }
    }

    @WorkerThread
    void cleanRecords() {
        if (mBatteryRecorder != null) {
            mBatteryRecorder.clean(DAY_LIMIT);
        }
    }

    public void statsAppStat(int appStat) {
        if (mBatteryStats != null) {
            writeRecord(mBatteryStats.statsAppStat(appStat));
        }
    }

    public void statsDevStat(int devStat) {
        if (mBatteryStats != null) {
            writeRecord(mBatteryStats.statsDevStat(devStat));
        }
    }

    public void statsScene(String scene) {
        if (mBatteryStats != null) {
            writeRecord(mBatteryStats.statsScene(scene));
        }
    }

    public void statsEvent(String event) {
        statsEvent(event, 0);
    }

    public void statsEvent(String event, int eventId) {
        statsEvent(event, eventId, Collections.<String, Object>emptyMap());
    }

    public void statsEvent(String event, int eventId, Map<String, Object> extras) {
        if (mBatteryStats != null) {
            writeRecord(mBatteryStats.statsEvent(event, eventId, extras));
        }
    }

    public void statsMonitors(CompositeMonitors monitors) {
        if (mBatteryRecorder == null) {
            return;
        }
        final AppStats appStats = monitors.getAppStats();
        if (appStats == null) {
            return;
        }
        if (mBatteryStats != null) {
            writeRecord(mBatteryStats.statsMonitors(monitors));
        }
    }

    public static String getDateString(int dayOffset) {
        return BatteryRecorder.MMKVRecorder.getDateString(dayOffset);
    }


    public static class BatteryRecords {
        public String date;
        public List<BatteryRecord> records;
    }
}
