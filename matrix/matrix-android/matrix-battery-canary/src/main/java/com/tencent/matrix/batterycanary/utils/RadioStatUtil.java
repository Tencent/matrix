package com.tencent.matrix.batterycanary.utils;

import android.Manifest;
import android.app.usage.NetworkStats;
import android.app.usage.NetworkStatsManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.os.Build;

import com.tencent.matrix.batterycanary.BuildConfig;
import com.tencent.matrix.util.MatrixLog;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.RestrictTo;
import androidx.core.util.Pair;

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
    static RadioStat sLastRef;

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

        // if (checkIfFrequently()) {
        //     return sLastRef;
        // }

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
                            stat.wifiRxPackets += bucket.getRxPackets();
                            stat.wifiTxPackets += bucket.getTxPackets();
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
                            stat.mobileRxPackets += bucket.getRxPackets();
                            stat.mobileTxPackets += bucket.getTxPackets();
                        }
                    }
                }
            }
            sLastRef = stat;
            return stat;

        } catch (Throwable e) {
            MatrixLog.w(TAG, "querySummary fail: " + e.getMessage());
            sLastRef = null;
            return null;
        }
    }

    @Nullable
    public static RadioBps getCurrentBps(Context context) {
        if (android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return null;
        }
        RadioBps stat = new RadioBps();
        Pair<Long, Long> wifi = getCurrentBps(context, "WIFI");
        stat.wifiRxBps = wifi.first == null ? 0 : wifi.first;
        stat.wifiTxBps = wifi.second == null ? 0 : wifi.second;

        Pair<Long, Long> mobile = getCurrentBps(context, "MOBILE");
        stat.mobileRxBps = mobile.first == null ? 0 : mobile.first;
        stat.mobileTxBps = mobile.second == null ? 0 : mobile.second;
        return stat;
    }

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    private static Pair<Long, Long> getCurrentBps(Context context, String typeName) {
        long rxBwBps = 0, txBwBps = 0;
        if (context.checkCallingOrSelfPermission(Manifest.permission.ACCESS_NETWORK_STATE) == PackageManager.PERMISSION_GRANTED) {
            ConnectivityManager manager = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
            for (Network item : manager.getAllNetworks()) {
                NetworkInfo networkInfo = manager.getNetworkInfo(item);
                if (networkInfo != null
                        && (networkInfo.isConnected() || networkInfo.isConnectedOrConnecting())
                        && networkInfo.getTypeName().equalsIgnoreCase(typeName)) {
                    NetworkCapabilities capabilities = manager.getNetworkCapabilities(item);
                    if (capabilities != null) {
                        rxBwBps = capabilities.getLinkDownstreamBandwidthKbps() * 1024L;
                        txBwBps = capabilities.getLinkUpstreamBandwidthKbps() * 1024L;
                        if (rxBwBps > 0 || txBwBps > 0) {
                            break;
                        }
                    }
                }
            }
        }
        return new Pair<>(rxBwBps, txBwBps);
    }

    public static final class RadioStat {
        public long wifiRxBytes;
        public long wifiTxBytes;
        public long wifiRxPackets;
        public long wifiTxPackets;

        public long mobileRxBytes;
        public long mobileTxBytes;
        public long mobileRxPackets;
        public long mobileTxPackets;
    }

    public static final class RadioBps {
        public long wifiRxBps;
        public long wifiTxBps;

        public long mobileRxBps;
        public long mobileTxBps;
    }
}
