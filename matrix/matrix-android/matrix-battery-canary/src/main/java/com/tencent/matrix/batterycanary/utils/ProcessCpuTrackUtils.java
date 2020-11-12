package com.tencent.matrix.batterycanary.utils;

import android.support.annotation.WorkerThread;
import android.text.TextUtils;

/**
 * see {@linkplain com.android.internal.os.ProcessCpuTracker}
 *
 * @author Kaede
 * @since 2020/11/6
 */
@SuppressWarnings("JavadocReference")
public final class ProcessCpuTrackUtils {

    @WorkerThread
    public static CpuLoad getCpuLoad() {
        CpuLoad cpuLoad = new CpuLoad();
        String cat = BatteryCanaryUtil.cat("/proc/loadavg");
        if (!TextUtils.isEmpty(cat)) {
            //noinspection ConstantConditions
            String[] splits = cat.split(" ");
            cpuLoad.load1 = Float.parseFloat(splits[0]);
            cpuLoad.load5 = Float.parseFloat(splits[1]);
            cpuLoad.load15 = Float.parseFloat(splits[2]);
        }
        return cpuLoad;
    }

    public static class CpuLoad {
        public float load1;
        public float load5;
        public float load15;
    }
}
