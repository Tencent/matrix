package com.tencent.matrix.batterycanary.stats;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Parcel;
import android.os.Parcelable;
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

import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;

/**
 * @author Kaede
 * @since 2021/12/3
 */
public final class BatteryStatsFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.BatteryStats";

    public interface BatteryRecorder {
        void write(String date, Record record);
        List<Record> read(String date);
        void clean(String date);
    }

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
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        mBatteryRecorder = null;
        mStatsHandler = null;
        if (mStatsThread != null) {
            mStatsThread.quit();
            mStatsThread = null;
        }
    }

    @VisibleForTesting
    static String getDateString(int dayOffset) {
        DateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd", Locale.getDefault());
        Calendar cal = Calendar.getInstance();
        cal.add(Calendar.DATE, dayOffset);
        return dateFormat.format(cal.getTime());
    }

    @WorkerThread
    void writeRecord(String date, Record record) {
        if (mBatteryRecorder != null) {
            mBatteryRecorder.write(date, record);
        }
    }

    @WorkerThread
    public List<Record> readRecords(String date) {
        if (mBatteryRecorder != null) {
            mBatteryRecorder.read(date);
        }
        return Collections.emptyList();
    }

    @WorkerThread
    void cleanRecords(String date) {
        if (mBatteryRecorder != null) {
            mBatteryRecorder.clean(date);
        }
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

    public static BatteryRecords loadBatteryRecords(int dayOffset) {
        BatteryRecords batteryRecords = new BatteryRecords();
        batteryRecords.date = getDateString(dayOffset);
        batteryRecords.records = loadRecords(dayOffset);
        return batteryRecords;
    }

    public static class BatteryRecords {
        public String date;
        public List<Record> records;
    }

    public static abstract class Record implements Parcelable {
        public static final int RECORD_TYPE_PROC_STAT = 1;
        public static final int RECORD_TYPE_DEV_STAT = 2;
        public static final int RECORD_TYPE_APP_STAT = 3;
        public static final int RECORD_TYPE_SCENE_STAT = 4;
        public static final int RECORD_TYPE_EVENT_STAT = 5;
        public static final int RECORD_TYPE_REPORT = 6;

        public int version;
        public long millis = System.currentTimeMillis();

        public static byte[] encode(Record record) {
            int type;
            Class<? extends Record> recordClass = record.getClass();
            if (recordClass == ProcStatRecord.class) {
                type = RECORD_TYPE_PROC_STAT;
            } else if (recordClass == DevStatRecord.class) {
                type = RECORD_TYPE_DEV_STAT;
            } else if (recordClass == AppStatRecord.class) {
                type = RECORD_TYPE_APP_STAT;
            } else if (recordClass == SceneStatRecord.class) {
                type = RECORD_TYPE_SCENE_STAT;
            } else if (recordClass == EventStatRecord.class) {
                type = RECORD_TYPE_EVENT_STAT;
            } else if (recordClass == ReportRecord.class) {
                type = RECORD_TYPE_REPORT;
            } else {
                throw new UnsupportedOperationException("Unknown record type: " + record);
            }

            Parcel parcel = null;
            try {
                parcel = Parcel.obtain();
                parcel.writeInt(type);
                record.writeToParcel(parcel, 0);
                return parcel.marshall();
            } finally {
                if (parcel != null) {
                    parcel.recycle();
                }
            }
        }

        @SuppressWarnings("ConstantConditions")
        public static Record decode(byte[] bytes) {
            Parcel parcel = null;
            try {
                parcel = Parcel.obtain();
                parcel.unmarshall(bytes, 0, bytes.length);
                parcel.setDataPosition(0);
                int type = parcel.readInt();

                Creator<?> creator;
                switch (type) {
                    case RECORD_TYPE_PROC_STAT:
                        creator = ProcStatRecord.CREATOR;
                        break;
                    case RECORD_TYPE_DEV_STAT:
                        creator = DevStatRecord.CREATOR;
                        break;
                    case RECORD_TYPE_APP_STAT:
                        creator = AppStatRecord.CREATOR;
                        break;
                    case RECORD_TYPE_SCENE_STAT:
                        creator = SceneStatRecord.CREATOR;
                        break;
                    case RECORD_TYPE_EVENT_STAT:
                        creator = EventStatRecord.CREATOR;
                        break;
                    case RECORD_TYPE_REPORT:
                        creator = ReportRecord.CREATOR;
                        break;
                    default:
                        throw new UnsupportedOperationException("Unknown record type: " + type);
                }
                return (Record) creator.createFromParcel(parcel);
            } finally {
                parcel.recycle();
            }
        }

        public static class ProcStatRecord extends Record implements Parcelable {
            public static final int VERSION = 0;
            public static final int STAT_PROC_LAUNCH = 1;

            public int procStat = STAT_PROC_LAUNCH;
            public int pid;

            public ProcStatRecord() {
                version = VERSION;
            }

            protected ProcStatRecord(Parcel in) {
                version = in.readInt();
                millis = in.readLong();
                procStat = in.readInt();
                pid = in.readInt();
            }

            public static final Creator<ProcStatRecord> CREATOR = new Creator<ProcStatRecord>() {
                @Override
                public ProcStatRecord createFromParcel(Parcel in) {
                    return new ProcStatRecord(in);
                }

                @Override
                public ProcStatRecord[] newArray(int size) {
                    return new ProcStatRecord[size];
                }
            };

            @Override
            public int describeContents() {
                return 0;
            }

            @Override
            public void writeToParcel(Parcel dest, int flags) {
                dest.writeInt(version);
                dest.writeLong(millis);
                dest.writeInt(procStat);
                dest.writeInt(pid);
            }
        }

        public static class DevStatRecord extends Record implements Parcelable {
            public static final int VERSION = 0;

            @AppStats.DevStatusDef
            public int devStat;

            public DevStatRecord() {
                version = VERSION;
            }

            protected DevStatRecord(Parcel in) {
                version = in.readInt();
                millis = in.readLong();
                devStat = in.readInt();
            }

            public static final Creator<DevStatRecord> CREATOR = new Creator<DevStatRecord>() {
                @Override
                public DevStatRecord createFromParcel(Parcel in) {
                    return new DevStatRecord(in);
                }

                @Override
                public DevStatRecord[] newArray(int size) {
                    return new DevStatRecord[size];
                }
            };

            @Override
            public int describeContents() {
                return 0;
            }

            @Override
            public void writeToParcel(Parcel dest, int flags) {
                dest.writeInt(version);
                dest.writeLong(millis);
                dest.writeInt(devStat);
            }
        }

        public static class AppStatRecord extends Record implements Parcelable {
            public static final int VERSION = 0;

            @AppStats.AppStatusDef
            public int appStat;

            public AppStatRecord() {
                version = VERSION;
            }

            protected AppStatRecord(Parcel in) {
                version = in.readInt();
                millis = in.readLong();
                appStat = in.readInt();
            }

            public static final Creator<AppStatRecord> CREATOR = new Creator<AppStatRecord>() {
                @Override
                public AppStatRecord createFromParcel(Parcel in) {
                    return new AppStatRecord(in);
                }

                @Override
                public AppStatRecord[] newArray(int size) {
                    return new AppStatRecord[size];
                }
            };

            @Override
            public int describeContents() {
                return 0;
            }

            @Override
            public void writeToParcel(Parcel dest, int flags) {
                dest.writeInt(version);
                dest.writeLong(millis);
                dest.writeInt(appStat);
            }
        }

        public static class SceneStatRecord extends Record implements Parcelable {
            public static final int VERSION = 0;

            public String scene;

            public SceneStatRecord() {
                version = VERSION;
            }

            protected SceneStatRecord(Parcel in) {
                version = in.readInt();
                millis = in.readLong();
                scene = in.readString();
            }

            public static final Creator<SceneStatRecord> CREATOR = new Creator<SceneStatRecord>() {
                @Override
                public SceneStatRecord createFromParcel(Parcel in) {
                    return new SceneStatRecord(in);
                }

                @Override
                public SceneStatRecord[] newArray(int size) {
                    return new SceneStatRecord[size];
                }
            };

            @Override
            public int describeContents() {
                return 0;
            }

            @Override
            public void writeToParcel(Parcel dest, int flags) {
                dest.writeInt(version);
                dest.writeLong(millis);
                dest.writeString(scene);
            }
        }


        public static class EventStatRecord extends Record implements Parcelable {
            public static final int VERSION = 0;

            public long id;
            public String event;

            public EventStatRecord() {
                version = VERSION;
            }

            protected EventStatRecord(Parcel in) {
                version = in.readInt();
                millis = in.readLong();
                id = in.readLong();
                event = in.readString();
            }

            public static final Creator<EventStatRecord> CREATOR = new Creator<EventStatRecord>() {
                @Override
                public EventStatRecord createFromParcel(Parcel in) {
                    return new EventStatRecord(in);
                }

                @Override
                public EventStatRecord[] newArray(int size) {
                    return new EventStatRecord[size];
                }
            };

            @Override
            public int describeContents() {
                return 0;
            }

            @Override
            public void writeToParcel(Parcel dest, int flags) {
                dest.writeInt(version);
                dest.writeLong(millis);
                dest.writeLong(id);
                dest.writeString(event);
            }
        }


        public static class ReportRecord extends EventStatRecord implements Parcelable {
            public static final int VERSION = 0;

            public String scope;
            public long windowMillis;
            public List<ThreadInfo> threadInfoList = Collections.emptyList();
            public List<EntryInfo> entryList = Collections.emptyList();

            public ReportRecord() {
                version = VERSION;
            }

            protected ReportRecord(Parcel in) {
                millis = in.readLong();
                id = in.readLong();
                event = in.readString();
                scope = in.readString();
                windowMillis = in.readLong();
                threadInfoList = in.createTypedArrayList(ThreadInfo.CREATOR);
                entryList = in.createTypedArrayList(EntryInfo.CREATOR);
            }

            public static final Creator<ReportRecord> CREATOR = new Creator<ReportRecord>() {
                @Override
                public ReportRecord createFromParcel(Parcel in) {
                    return new ReportRecord(in);
                }

                @Override
                public ReportRecord[] newArray(int size) {
                    return new ReportRecord[size];
                }
            };

            @Override
            public int describeContents() {
                return 0;
            }

            @Override
            public void writeToParcel(Parcel dest, int flags) {
                dest.writeLong(millis);
                dest.writeLong(id);
                dest.writeString(event);
                dest.writeString(scope);
                dest.writeLong(windowMillis);
                dest.writeTypedList(threadInfoList);
                dest.writeTypedList(entryList);
            }


            public static class ThreadInfo implements Parcelable {
                public int tid;
                public String name;
                public String stat;
                public long jiffies;
                public Map<String, String> extraInfo = Collections.emptyMap();

                public ThreadInfo() {
                }

                protected ThreadInfo(Parcel in) {
                    tid = in.readInt();
                    name = in.readString();
                    stat = in.readString();
                    jiffies = in.readLong();
                    extraInfo = new ArrayMap<>();
                    in.readMap(extraInfo, getClass().getClassLoader());
                }

                public static final Creator<ThreadInfo> CREATOR = new Creator<ThreadInfo>() {
                    @Override
                    public ThreadInfo createFromParcel(Parcel in) {
                        return new ThreadInfo(in);
                    }

                    @Override
                    public ThreadInfo[] newArray(int size) {
                        return new ThreadInfo[size];
                    }
                };

                @Override
                public int describeContents() {
                    return 0;
                }

                @Override
                public void writeToParcel(Parcel dest, int flags) {
                    dest.writeInt(tid);
                    dest.writeString(name);
                    dest.writeString(stat);
                    dest.writeLong(jiffies);
                    dest.writeMap(extraInfo);
                }
            }

            public static class EntryInfo implements Parcelable {
                public String name;
                public String stat;
                public Map<String, String> entries = Collections.emptyMap();
                public Map<String, String> extraInfo = Collections.emptyMap();

                public EntryInfo() {
                }

                protected EntryInfo(Parcel in) {
                    name = in.readString();
                    stat = in.readString();
                    entries  = new ArrayMap<>();
                    in.readMap(entries, getClass().getClassLoader());
                    extraInfo  = new ArrayMap<>();
                    in.readMap(extraInfo, getClass().getClassLoader());
                }

                public static final Creator<EntryInfo> CREATOR = new Creator<EntryInfo>() {
                    @Override
                    public EntryInfo createFromParcel(Parcel in) {
                        return new EntryInfo(in);
                    }

                    @Override
                    public EntryInfo[] newArray(int size) {
                        return new EntryInfo[size];
                    }
                };

                @Override
                public int describeContents() {
                    return 0;
                }

                @Override
                public void writeToParcel(Parcel dest, int flags) {
                    dest.writeString(name);
                    dest.writeString(stat);
                    dest.writeMap(entries);
                    dest.writeMap(extraInfo);
                }
            }
        }
    }
}
