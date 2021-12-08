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
import java.util.Map;

import androidx.annotation.WorkerThread;

/**
 * @author Kaede
 * @since 2021/12/3
 */
public final class BatteryStatsFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.BatteryStats";

    public interface BatteryRecorder {
        void write(Record record);
    }

    private HandlerThread mStatsThread;
    private Handler mStatsHandler;

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
        mStatsThread = MatrixHandlerThread.getNewHandlerThread("matrix-stats", Thread.NORM_PRIORITY);
        mStatsHandler = new Handler(mStatsThread.getLooper());
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        mStatsHandler = null;
        if (mStatsThread != null) {
            mStatsThread.quit();
            mStatsThread = null;
        }
    }

    String getDateString(int dayOffset) {
        DateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd", Locale.getDefault());
        Calendar cal = Calendar.getInstance();
        cal.add(Calendar.DATE, dayOffset);
        return dateFormat.format(cal.getTime());
    }

    @WorkerThread
    void onWrite(Record record) {

    }

    public static List<Record> loadRecords(int dayOffset) {
        List<Record> records = new ArrayList<>();
        Record.ProcStatRecord procStatRecord = new Record.ProcStatRecord();
        procStatRecord.pid = Process.myPid();
        records.add(procStatRecord);

        Record.AppStatRecord appStat = new Record.AppStatRecord();
        appStat.appStat = AppStats.APP_STAT_FOREGROUND;
        records.add(appStat);

        Record.SceneStatRecord sceneStatRecord = new Record.SceneStatRecord();
        sceneStatRecord.scene = "Activity 1";
        records.add(sceneStatRecord);
        sceneStatRecord = new Record.SceneStatRecord();
        sceneStatRecord.scene = "Activity 2";
        records.add(sceneStatRecord);
        sceneStatRecord = new Record.SceneStatRecord();
        sceneStatRecord.scene = "Activity 3";
        records.add(sceneStatRecord);

        appStat = new Record.AppStatRecord();
        appStat.appStat = AppStats.APP_STAT_BACKGROUND;
        records.add(appStat);

        Record.DevStatRecord devStatRecord = new Record.DevStatRecord();
        devStatRecord.devStat = AppStats.DEV_STAT_CHARGING;
        records.add(devStatRecord);
        devStatRecord = new Record.DevStatRecord();
        devStatRecord.devStat = AppStats.DEV_STAT_UN_CHARGING;
        records.add(devStatRecord);

        appStat = new Record.AppStatRecord();
        appStat.appStat = AppStats.APP_STAT_FOREGROUND;
        records.add(appStat);

        Record.ReportRecord reportRecord = new Record.ReportRecord();
        reportRecord.threadInfoList = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
           Record.ReportRecord.ThreadInfo threadInfo = new Record.ReportRecord.ThreadInfo();
            threadInfo.stat = "R" + i;
            threadInfo.tid = 10000 + i;
            threadInfo.name = "ThreadName_" + i;
            threadInfo.jiffies = SystemClock.currentThreadTimeMillis() / 10;
            reportRecord.threadInfoList.add(0, threadInfo);
        }
        reportRecord.entryList = new ArrayList<>();
        for (int i = 0; i < 4; i++) {
            Record.ReportRecord.EntryInfo entryInfo = new Record.ReportRecord.EntryInfo();
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

    public static class Record {
        public long millis = System.currentTimeMillis();

        public static class ProcStatRecord extends Record {
            public static final int STAT_PROC_LAUNCH = 1;
            public int procStat = STAT_PROC_LAUNCH;
            public int pid;
        }

        public static class DevStatRecord extends Record {
            @AppStats.DevStatusDef
            public int devStat;
        }

        public static class AppStatRecord extends Record {
            @AppStats.AppStatusDef
            public int appStat;
        }

        public static class SceneStatRecord extends Record {
            public String scene;
        }

        public static class EventStatRecord extends Record {
            public long id;
            public String event;
        }

        public static class ReportRecord extends EventStatRecord {
            public String scope;
            public long windowMillis;
            public List<ThreadInfo> threadInfoList = Collections.emptyList();
            public List<EntryInfo> entryList = Collections.emptyList();


            public static class ThreadInfo {
                public int tid;
                public String name;
                public String stat;
                public long jiffies;
                public Map<String, String> extraInfo = Collections.emptyMap();
            }

            public static class EntryInfo {
                public String name;
                public String stat;
                public Map<String, String> entries = Collections.emptyMap();
                public Map<String, String> extraInfo = Collections.emptyMap();
            }
        }
    }
}
