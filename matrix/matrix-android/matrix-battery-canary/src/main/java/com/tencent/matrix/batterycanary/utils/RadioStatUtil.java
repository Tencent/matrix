package com.tencent.matrix.batterycanary.utils;

import android.app.usage.NetworkStats;
import android.app.usage.NetworkStatsManager;
import android.content.Context;
import android.net.NetworkCapabilities;
import androidx.annotation.Nullable;
import androidx.annotation.RestrictTo;

import com.tencent.matrix.batterycanary.BuildConfig;
import com.tencent.matrix.util.MatrixLog;

/**
 * @author Kaede
 * @since 2020/12/8
 */
@SuppressWarnings("SpellCheckingInspection")
@RestrictTo(RestrictTo.Scope.LIBRARY)
public final class RadioStatUtil {
    private static final String TAG = "Matrix.battery.ProcStatUtil";
    static final long MIN_QUERY_INTERVAL = BuildConfig.DEBUG ? 0L : 2000L;
    static long sLastQueryMillis;

    private static boolean checkIfFrequently() {
        long currentTimeMillis = System.currentTimeMillis();
        if (currentTimeMillis - sLastQueryMillis < MIN_QUERY_INTERVAL) {
            return true;
        }
        sLastQueryMillis = currentTimeMillis;
        return false;
    }

    @Nullable
    public static RadioStat getCurrentStat(Context context) {
        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.M) {
            return null;
        }
        if (checkIfFrequently()) {
            MatrixLog.i(TAG, "over frequently just return");
            return null;
        }

        try {
            NetworkStatsManager network = (NetworkStatsManager) context.getSystemService(Context.NETWORK_STATS_SERVICE);
            if (network == null) {
                return null;
            }
            RadioStat stat = new RadioStat();
            try (NetworkStats stats = network.querySummary(NetworkCapabilities.TRANSPORT_WIFI, null, 0, System.currentTimeMillis())) {
                while (stats.hasNextBucket()) {
                    NetworkStats.Bucket bucket = new NetworkStats.Bucket();
                    if (stats.getNextBucket(bucket)) {
                        if (bucket.getUid() == android.os.Process.myUid()) {
                            stat.wifiRxBytes += bucket.getRxBytes();
                            stat.wifiTxBytes += bucket.getTxBytes();
                        }
                    }
                }
            }
            try (NetworkStats stats = network.querySummary(NetworkCapabilities.TRANSPORT_CELLULAR, null, 0, System.currentTimeMillis())) {
                while (stats.hasNextBucket()) {
                    NetworkStats.Bucket bucket = new NetworkStats.Bucket();
                    if (stats.getNextBucket(bucket)) {
                        if (bucket.getUid() == android.os.Process.myUid()) {
                            stat.mobileRxBytes += bucket.getRxBytes();
                            stat.mobileTxBytes += bucket.getTxBytes();
                        }
                    }
                }
            }

            return stat;

        } catch (Throwable e) {
            MatrixLog.w(TAG, "querySummary fail: " + e.getMessage());
            return null;
        }
    }

    public static final class RadioStat {
        public long wifiRxBytes;
        public long wifiTxBytes;
        public long mobileRxBytes;
        public long mobileTxBytes;
    }
}
