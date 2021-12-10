package com.tencent.matrix.batterycanary.stats;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Process;
import android.os.SystemClock;
import android.util.ArrayMap;

import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.monitor.feature.AbsMonitorFeature;
import com.tencent.matrix.util.MatrixHandlerThread;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;

/**
 * @author Kaede
 * @since 2021/12/3
 */
public final class BatteryStatsFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.BatteryStats";

    private HandlerThread mStatsThread;
    private Handler mStatsHandler;
    private BatteryRecorder mBatteryRecorder;

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
        if (mBatteryRecorder != null) {
            mStatsThread = MatrixHandlerThread.getNewHandlerThread("matrix-stats", Thread.NORM_PRIORITY);
            mStatsHandler = new Handler(mStatsThread.getLooper());
        }

        BatteryRecord.ProcStatRecord procStatRecord = new BatteryRecord.ProcStatRecord();
        procStatRecord.pid = Process.myPid();
        procStatRecord.procStat = BatteryRecord.ProcStatRecord.STAT_PROC_LAUNCH;
        writeRecord(procStatRecord);
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

    public void writeRecord(final BatteryRecord record) {
        if (mBatteryRecorder != null) {
            if (mStatsHandler != null) {
                final String date = getDateString(0);
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
    public List<BatteryRecord> readRecords(int dayOffset) {
        if (mBatteryRecorder != null) {
            final String date = getDateString(dayOffset);
            return mBatteryRecorder.read(date);
        }
        return Collections.emptyList();
    }

    @WorkerThread
    void writeRecord(String date, BatteryRecord record) {
        if (mBatteryRecorder != null) {
            mBatteryRecorder.write(date, record);
        }
    }

    @WorkerThread
    List<BatteryRecord> readRecords(String date) {
        if (mBatteryRecorder != null) {
            return mBatteryRecorder.read(date);
        }
        return Collections.emptyList();
    }

    @WorkerThread
    void cleanRecords(String date) {
        if (mBatteryRecorder != null) {
            mBatteryRecorder.clean(date);
        }
    }

    @VisibleForTesting
    static String getDateString(int dayOffset) {
        DateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd", Locale.getDefault());
        Calendar cal = Calendar.getInstance();
        cal.add(Calendar.DATE, dayOffset);
        return dateFormat.format(cal.getTime());
    }

    public static List<BatteryRecord> loadRecords(int dayOffset) {
        List<BatteryRecord> records = new ArrayList<>();
        BatteryRecord.ProcStatRecord procStatRecord = new BatteryRecord.ProcStatRecord();
        procStatRecord.pid = Process.myPid();
        records.add(procStatRecord);

        BatteryRecord.AppStatRecord appStat = new BatteryRecord.AppStatRecord();
        appStat.appStat = AppStats.APP_STAT_FOREGROUND;
        records.add(appStat);

        BatteryRecord.SceneStatRecord sceneStatRecord = new BatteryRecord.SceneStatRecord();
        sceneStatRecord.scene = "Activity 1";
        records.add(sceneStatRecord);
        sceneStatRecord = new BatteryRecord.SceneStatRecord();
        sceneStatRecord.scene = "Activity 2";
        records.add(sceneStatRecord);
        sceneStatRecord = new BatteryRecord.SceneStatRecord();
        sceneStatRecord.scene = "Activity 3";
        records.add(sceneStatRecord);

        appStat = new BatteryRecord.AppStatRecord();
        appStat.appStat = AppStats.APP_STAT_BACKGROUND;
        records.add(appStat);

        BatteryRecord.DevStatRecord devStatRecord = new BatteryRecord.DevStatRecord();
        devStatRecord.devStat = AppStats.DEV_STAT_CHARGING;
        records.add(devStatRecord);
        devStatRecord = new BatteryRecord.DevStatRecord();
        devStatRecord.devStat = AppStats.DEV_STAT_UN_CHARGING;
        records.add(devStatRecord);

        appStat = new BatteryRecord.AppStatRecord();
        appStat.appStat = AppStats.APP_STAT_FOREGROUND;
        records.add(appStat);

        BatteryRecord.ReportRecord reportRecord = new BatteryRecord.ReportRecord();
        reportRecord.threadInfoList = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
           BatteryRecord.ReportRecord.ThreadInfo threadInfo = new BatteryRecord.ReportRecord.ThreadInfo();
            threadInfo.stat = "R" + i;
            threadInfo.tid = 10000 + i;
            threadInfo.name = "ThreadName_" + i;
            threadInfo.jiffies = SystemClock.currentThreadTimeMillis() / 10;
            reportRecord.threadInfoList.add(0, threadInfo);
        }
        reportRecord.entryList = new ArrayList<>();
        for (int i = 0; i < 4; i++) {
            BatteryRecord.ReportRecord.EntryInfo entryInfo = new BatteryRecord.ReportRecord.EntryInfo();
            entryInfo.name = "Entry Name " + i;
            entryInfo.entries = new ArrayMap<>();
            entryInfo.entries.put("Key 1", "Value 1");
            entryInfo.entries.put("Key 2", "Value 2");
            entryInfo.entries.put("Key 3", "Value 3");
            reportRecord.entryList.add(entryInfo);
        }
        records.add(reportRecord);
        return records;
    }

    public static BatteryRecords loadBatteryRecords(int dayOffset) {
        BatteryRecords batteryRecords = new BatteryRecords();
        batteryRecords.date = getDateString(dayOffset);
        batteryRecords.records = loadRecords(dayOffset);
        return batteryRecords;
    }

    public static class BatteryRecords {
        public String date;
        public List<BatteryRecord> records;
    }

}
