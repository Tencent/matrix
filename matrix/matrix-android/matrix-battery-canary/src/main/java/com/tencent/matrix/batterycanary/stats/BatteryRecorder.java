package com.tencent.matrix.batterycanary.stats;

import android.os.Process;
import android.text.TextUtils;

import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.mmkv.MMKV;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * @author Kaede
 * @since 2021/12/10
 */
public interface BatteryRecorder {
    String TAG = "Matrix.battery.recorder";

    void updateProc(String proc);

    Set<String> getProcSet();

    void write(String date, BatteryRecord record);

    List<BatteryRecord> read(String date, String proc);

    void clean(String date, String proc);


    class MMKVRecorder implements BatteryRecorder {
        protected static final String MAGIC = "bs";
        protected static String sProcNameSuffix = null;

        protected final int pid = Process.myPid();
        protected final MMKV mmkv;
        protected AtomicInteger inc = new AtomicInteger(0);

        public MMKVRecorder(MMKV mmkv) {
            this.mmkv = mmkv;
        }

        protected String getKeyPrefix(String date, String proc) {
            return MAGIC + "-" + date + (TextUtils.isEmpty(proc) ? "" : "-" + proc);
        }

        @Override
        public void updateProc(String proc) {
            if (!TextUtils.isEmpty(proc)) {
                Set<String> procSet = getProcSet();
                if (!procSet.contains(proc)) {
                    if (procSet.isEmpty()) {
                        procSet = new HashSet<>();
                    }
                    procSet.add(proc);
                    String key = MAGIC + "-proc-set";
                    mmkv.encode(key, procSet);
                }
            }
        }

        @Override
        public Set<String> getProcSet() {
            String key = MAGIC + "-proc-set";
            Set<String> procSet = mmkv.decodeStringSet(key);
            return procSet == null ? Collections.<String>emptySet() : procSet;
        }

        @Override
        public void write(String date, BatteryRecord record) {
            String proc = getProcNameSuffix();
            write(date, record, proc);
        }

        public void write(String date, BatteryRecord record, String proc) {
            try {
                String key = getKeyPrefix(date, proc) + "-" + pid + "-" + inc.getAndIncrement();
                byte[] bytes = BatteryRecord.encode(record);
                mmkv.encode(key, bytes);
                mmkv.sync();
            } catch (Exception e) {
                MatrixLog.w(TAG, "record encode failed: " + e.getMessage());
            }
        }

        @Override
        public List<BatteryRecord> read(String date, String proc) {
            String[] keys = mmkv.allKeys();
            if (keys == null || keys.length == 0) {
                return Collections.emptyList();
            }
            List<BatteryRecord> records = new ArrayList<>(Math.min(16, keys.length));
            String keyPrefix = getKeyPrefix(date, proc);
            for (String item : keys) {
                if (item.startsWith(keyPrefix)) {
                    try {
                        byte[] bytes = mmkv.decodeBytes(item);
                        if (bytes != null) {
                            BatteryRecord record = BatteryRecord.decode(bytes);
                            records.add(record);
                        }
                    } catch (Exception e) {
                        MatrixLog.w(TAG, "record decode failed: " + e.getMessage());
                    }
                }
            }
            Collections.sort(records, new Comparator<BatteryRecord>() {
                @Override
                public int compare(BatteryRecord left, BatteryRecord right) {
                    return Long.compare(left.millis, right.millis);
                }
            });
            return records;
        }

        @Override
        public void clean(String date, String proc) {
            String[] keys = mmkv.allKeys();
            if (keys == null || keys.length == 0) {
                return;
            }
            String keyPrefix = getKeyPrefix(date, proc);
            for (String item : keys) {
                if (item.startsWith(keyPrefix)) {
                    try {
                        mmkv.remove(item);
                    } catch (Exception e) {
                        MatrixLog.w(TAG, "record clean failed: " + e.getMessage());
                    }
                }
            }
        }

        public void flush() {
            mmkv.sync();
        }

        public static String getProcNameSuffix() {
            if (sProcNameSuffix == null) {
                String processName = BatteryCanaryUtil.getProcessName();
                if (processName.contains(":")) {
                    sProcNameSuffix = processName.substring(processName.lastIndexOf(":") + 1);
                } else {
                    sProcNameSuffix = "main";
                }
            }
            return sProcNameSuffix;
        }
    }
}
