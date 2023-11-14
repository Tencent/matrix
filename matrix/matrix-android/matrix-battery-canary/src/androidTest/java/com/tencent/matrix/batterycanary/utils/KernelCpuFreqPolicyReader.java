package com.tencent.matrix.batterycanary.utils;


import android.text.TextUtils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;


public class KernelCpuFreqPolicyReader {
    private static final String TAG = "KernelCpuSpeedReader";

    public static List<KernelCpuFreqPolicyReader> read() {
        List<KernelCpuFreqPolicyReader> list = new ArrayList<>();
        File file = new File("/sys/devices/system/cpu/cpufreq/");
        File[] files = file.listFiles();
        if (files != null) {
            for (File item : files) {
                if (item.isDirectory() && item.getName().startsWith("policy") && item.canRead()) {
                    list.add(new KernelCpuFreqPolicyReader(item.getName()));
                }
            }
        }
        return list;
    }

    private final String mPolicy;
    private final String mProcFile;

    public KernelCpuFreqPolicyReader(String policy) {
        mPolicy = policy;
        mProcFile = "/sys/devices/system/cpu/cpufreq/" + policy + "/stats/time_in_state";
    }

    public long readTotal() throws IOException {
        long sum = 0;
        for (long item : readAbsolute()) {
            sum += item;
        }
        return sum;
    }

    public List<Long> readAbsolute() throws IOException {
        List<Long> speedTimeJiffies = new ArrayList<>();
        try (BufferedReader reader = new BufferedReader(new FileReader(mProcFile))) {
            TextUtils.SimpleStringSplitter splitter = new TextUtils.SimpleStringSplitter(' ');
            String line;
            while ((line = reader.readLine()) != null) {
                splitter.setString(line);
                splitter.next();
                speedTimeJiffies.add(Long.parseLong(splitter.next()));
            }
        } catch (Throwable e) {
            throw new IOException("Failed to read cpu-freq time: " + e.getMessage(), e);
        }
        return speedTimeJiffies;
    }
}