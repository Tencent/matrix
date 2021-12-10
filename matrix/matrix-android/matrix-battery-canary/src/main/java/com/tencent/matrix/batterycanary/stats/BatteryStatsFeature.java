package com.tencent.matrix.batterycanary.stats;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Process;

import com.tencent.matrix.batterycanary.monitor.feature.AbsMonitorFeature;
import com.tencent.matrix.util.MatrixHandlerThread;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

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
    public BatteryRecords readBatteryRecords(int dayOffset) {
        BatteryRecords batteryRecords = new BatteryRecords();
        batteryRecords.date = getDateString(dayOffset);
        batteryRecords.records = readRecords(dayOffset);
        return batteryRecords;
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

    public static String getDateString(int dayOffset) {
        DateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd", Locale.getDefault());
        Calendar cal = Calendar.getInstance();
        cal.add(Calendar.DATE, dayOffset);
        return dateFormat.format(cal.getTime());
    }


    public static class BatteryRecords {
        public String date;
        public List<BatteryRecord> records;
    }
}
