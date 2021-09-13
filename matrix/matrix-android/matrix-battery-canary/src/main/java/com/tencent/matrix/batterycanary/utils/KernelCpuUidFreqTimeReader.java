package com.tencent.matrix.batterycanary.utils;


import android.text.TextUtils;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import androidx.annotation.RestrictTo;

/**
 * Reads /proc/uid_time_in_state which has the format:
 * <p>
 * uid: [freq1] [freq2] [freq3] ...
 * [uid1]: [time in freq1] [time in freq2] [time in freq3] ...
 * [uid2]: [time in freq1] [time in freq2] [time in freq3] ...
 * ...
 * <p>
 * This provides the times a UID's processes spent executing at each different cpu frequency.
 * The file contains a monotonically increasing count of time for a single boot. This class
 * maintains the previous results of a call to {@link #readDelta} in order to provide a proper
 * delta.
 * <p>
 * where time is measured in jiffies.
 *
 * @see com.android.internal.os.KernelCpuUidTimeReader.KernelCpuUidFreqTimeReader
 */
@RestrictTo(RestrictTo.Scope.LIBRARY)
@SuppressWarnings({"SpellCheckingInspection", "JavadocReference"})
public class KernelCpuUidFreqTimeReader {
    private static final String TAG = "KernelCpuUidFreqTimeReader";

    private final String mProcFile;
    private final int[] mClusterSteps;

    public KernelCpuUidFreqTimeReader(int pid, int[] clusterSteps) {
        mProcFile = "/proc/" + pid + "/time_in_state";
        mClusterSteps = clusterSteps;
    }

    public void smoke() throws IOException {
        List<long[]> cpuCoreStepJiffies = readAbsolute();
        if (mClusterSteps.length != cpuCoreStepJiffies.size()) {
            throw new IOException("Cpu clusterNum unmatched, expect = " + mClusterSteps.length + ", actual = " + cpuCoreStepJiffies.size());
        }
        for (int i = 0; i < cpuCoreStepJiffies.size(); i++) {
            long[] clusterStepJiffies = cpuCoreStepJiffies.get(i);
            if (mClusterSteps[i] != clusterStepJiffies.length) {
                throw new IOException("Cpu clusterStepNum unmatched, expect = " + mClusterSteps[i] + ", actual = " + clusterStepJiffies.length + ", cluster = " + i);
            }
        }
    }

    public List<Long> readTotoal() throws IOException {
        List<long[]> cpuCoreStepJiffies = readAbsolute();
        List<Long> cpuCoreJiffies = new ArrayList<>(cpuCoreStepJiffies.size());
        for (long[] stepJiffies : cpuCoreStepJiffies) {
            long sum = 0;
            for (long item : stepJiffies) {
                sum += item;
            }
            cpuCoreJiffies.add(sum);
        }

        return cpuCoreJiffies;
    }

    public List<long[]> readAbsolute() throws IOException {
        List<long[]> cpuCoreJiffies = new ArrayList<>();
        long[] speedJiffies = null;
        try (BufferedReader reader = new BufferedReader(new FileReader(mProcFile))) {
            TextUtils.SimpleStringSplitter splitter = new TextUtils.SimpleStringSplitter(' ');
            String line;
            int cluster = -1;
            int speedIndex = 0;
            while ((line = reader.readLine()) != null) {
                if (line.startsWith("cpu")) {
                    if (cluster >= 0) {
                        cpuCoreJiffies.add(speedJiffies);
                    }
                    cluster++;
                    speedIndex = 0;
                    speedJiffies = new long[mClusterSteps[cluster]];
                    continue;
                }
                if (speedIndex < mClusterSteps[cluster]) {
                    splitter.setString(line);
                    splitter.next();
                    speedJiffies[speedIndex] = Long.parseLong(splitter.next());
                    speedIndex++;
                }
            }
            cpuCoreJiffies.add(speedJiffies);
        } catch (IOException e) {
            throw new IOException("Failed to read cpu-freq: " + e.getMessage(), e);
        }
        return cpuCoreJiffies;
    }
}