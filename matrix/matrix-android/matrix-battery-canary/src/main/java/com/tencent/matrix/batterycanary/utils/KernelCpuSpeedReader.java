package com.tencent.matrix.batterycanary.utils;


import android.text.TextUtils;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;

import androidx.annotation.RestrictTo;

/**
 * Reads CPU time of a specific core spent at various frequencies and provides a delta from the
 * last call to {@link #readDelta}. Each line in the proc file has the format:
 * <p>
 * freq time
 * <p>
 * where time is measured in jiffies.
 *
 * @see com.android.internal.os.KernelCpuSpeedReader
 */
@RestrictTo(RestrictTo.Scope.LIBRARY)
@SuppressWarnings({"SpellCheckingInspection", "JavadocReference"})
public class KernelCpuSpeedReader {
    private static final String TAG = "KernelCpuSpeedReader";

    private final String mProcFile;
    private final int mNumSpeedSteps;

    public KernelCpuSpeedReader(int cpuNumber, int numSpeedSteps) {
        mProcFile = "/sys/devices/system/cpu/cpu" + cpuNumber + "/cpufreq/stats/time_in_state";
        mNumSpeedSteps = numSpeedSteps;
    }

    public void smoke() throws IOException {
        long[] stepJiffies = readAbsolute();
        if (stepJiffies.length != mNumSpeedSteps) {
            throw new IOException("CpuCore Step unmatched, expect = " + mNumSpeedSteps + ", actual = " + stepJiffies.length + ", path = " + mProcFile);
        }
    }

    public long readTotal() throws IOException {
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
    public long[] readAbsolute() throws IOException {
        long[] speedTimeJiffies = new long[mNumSpeedSteps];
        try (BufferedReader reader = new BufferedReader(new FileReader(mProcFile))) {
            TextUtils.SimpleStringSplitter splitter = new TextUtils.SimpleStringSplitter(' ');
            String line;
            int speedIndex = 0;
            while (speedIndex < mNumSpeedSteps && (line = reader.readLine()) != null) {
                splitter.setString(line);
                splitter.next();
                speedTimeJiffies[speedIndex] = Long.parseLong(splitter.next());
                speedIndex++;
            }
        } catch (Throwable e) {
            throw new IOException("Failed to read cpu-freq: " + e.getMessage(), e);
        }
        return speedTimeJiffies;
    }
}