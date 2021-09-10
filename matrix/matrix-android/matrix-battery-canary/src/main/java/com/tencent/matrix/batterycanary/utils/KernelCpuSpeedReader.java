package com.tencent.matrix.batterycanary.utils;


import android.text.TextUtils;

import com.tencent.matrix.util.MatrixLog;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.Arrays;

/**
 * Reads CPU time of a specific core spent at various frequencies and provides a delta from the
 * last call to {@link #readDelta}. Each line in the proc file has the format:
 *
 * freq time
 *
 * where time is measured in jiffies.
 *
 * @see com.android.internal.os.KernelCpuSpeedReader
 */
@SuppressWarnings({"SpellCheckingInspection", "JavadocReference"})
public class KernelCpuSpeedReader {
    private static final String TAG = "KernelCpuSpeedReader";

    private final String mProcFile;
    private final int mNumSpeedSteps;

    public KernelCpuSpeedReader(int cpuNumber, int numSpeedSteps) {
        mProcFile = "/sys/devices/system/cpu/cpu" + cpuNumber + "/cpufreq/stats/time_in_state";
        mNumSpeedSteps = numSpeedSteps;
    }

    public long readTotoal() {
        long sum = 0;
        for (long item : readAbsolute()) {
            sum += item;
        }
        return sum;
    }

    /**
     * @return The time (in jiffies) spent at different cpu speeds. The values should be
     * monotonically increasing, unless the cpu was hotplugged.
     */
    public long[] readAbsolute() {
        long[] speedTimeMs = new long[mNumSpeedSteps];
        try (BufferedReader reader = new BufferedReader(new FileReader(mProcFile))) {
            TextUtils.SimpleStringSplitter splitter = new TextUtils.SimpleStringSplitter(' ');
            String line;
            int speedIndex = 0;
            while (speedIndex < mNumSpeedSteps && (line = reader.readLine()) != null) {
                splitter.setString(line);
                splitter.next();
                speedTimeMs[speedIndex] = Long.parseLong(splitter.next());
                speedIndex++;
            }
        } catch (IOException e) {
            MatrixLog.e(TAG, "Failed to read cpu-freq: " + e.getMessage());
            Arrays.fill(speedTimeMs, 0);
        }
        return speedTimeMs;
    }
}