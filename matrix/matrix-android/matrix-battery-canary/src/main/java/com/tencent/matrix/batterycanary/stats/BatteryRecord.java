package com.tencent.matrix.batterycanary.stats;

import android.os.Parcel;
import android.os.Parcelable;
import android.util.ArrayMap;

import com.tencent.matrix.batterycanary.monitor.AppStats;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * @author Kaede
 * @since 2021/12/10
 */
public abstract class BatteryRecord implements Parcelable {

    public static final int RECORD_TYPE_PROC_STAT = 1;
    public static final int RECORD_TYPE_DEV_STAT = 2;
    public static final int RECORD_TYPE_APP_STAT = 3;
    public static final int RECORD_TYPE_SCENE_STAT = 4;
    public static final int RECORD_TYPE_EVENT_STAT = 5;
    public static final int RECORD_TYPE_REPORT = 6;

    public int version;
    public long millis = System.currentTimeMillis();

    public static byte[] encode(BatteryRecord record) {
        int type;
        Class<? extends BatteryRecord> recordClass = record.getClass();
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
    public static BatteryRecord decode(byte[] bytes) {
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
            return (BatteryRecord) creator.createFromParcel(parcel);
        } finally {
            parcel.recycle();
        }
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        throw new RuntimeException("Stub!");
    }

    public static class ProcStatRecord extends BatteryRecord implements Parcelable {
        public static final int VERSION = 0;
        public static final int STAT_PROC_LAUNCH = 1;
        public static final int STAT_PROC_OFF = 2;

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

    public static class DevStatRecord extends BatteryRecord implements Parcelable {
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

    public static class AppStatRecord extends BatteryRecord implements Parcelable {
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

    public static class SceneStatRecord extends BatteryRecord implements Parcelable {
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


    public static class EventStatRecord extends BatteryRecord implements Parcelable {
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
        public List<ReportRecord.ThreadInfo> threadInfoList = Collections.emptyList();
        public List<ReportRecord.EntryInfo> entryList = Collections.emptyList();

        public ReportRecord() {
            version = VERSION;
        }

        protected ReportRecord(Parcel in) {
            millis = in.readLong();
            id = in.readLong();
            event = in.readString();
            scope = in.readString();
            windowMillis = in.readLong();
            threadInfoList = in.createTypedArrayList(ReportRecord.ThreadInfo.CREATOR);
            entryList = in.createTypedArrayList(ReportRecord.EntryInfo.CREATOR);
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

            public static final Creator<ReportRecord.ThreadInfo> CREATOR = new Creator<ReportRecord.ThreadInfo>() {
                @Override
                public ReportRecord.ThreadInfo createFromParcel(Parcel in) {
                    return new ReportRecord.ThreadInfo(in);
                }

                @Override
                public ReportRecord.ThreadInfo[] newArray(int size) {
                    return new ReportRecord.ThreadInfo[size];
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

            public EntryInfo() {
            }

            protected EntryInfo(Parcel in) {
                name = in.readString();
                stat = in.readString();
                entries = new LinkedHashMap<>();
                in.readMap(entries, getClass().getClassLoader());
            }

            public static final Creator<ReportRecord.EntryInfo> CREATOR = new Creator<ReportRecord.EntryInfo>() {
                @Override
                public ReportRecord.EntryInfo createFromParcel(Parcel in) {
                    return new ReportRecord.EntryInfo(in);
                }

                @Override
                public ReportRecord.EntryInfo[] newArray(int size) {
                    return new ReportRecord.EntryInfo[size];
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
            }
        }
    }
}
