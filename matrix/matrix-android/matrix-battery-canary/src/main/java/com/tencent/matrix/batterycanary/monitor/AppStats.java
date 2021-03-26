package com.tencent.matrix.batterycanary.monitor;

import android.support.annotation.Nullable;
import android.support.v4.util.Pair;

import com.tencent.matrix.batterycanary.BatteryCanary;
import com.tencent.matrix.batterycanary.monitor.feature.AppStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature;
import com.tencent.matrix.batterycanary.utils.TimeBreaker;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * @author Kaede
 * @since 2021/1/27
 */
final public class AppStats {
    public static final int ONE_MIN = 60 * 1000;

    public static final int APP_STAT_FOREGROUND = 1;
    public static final int APP_STAT_FOREGROUND_SERVICE = 3;
    public static final int APP_STAT_BACKGROUND = 2;

    public static final int DEV_STAT_CHARGING = 1;
    public static final int DEV_STAT_UN_CHARGING = 2;
    public static final int DEV_STAT_SCREEN_OFF = 3;
    public static final int DEV_STAT_SAVE_POWER_MODE = 4;

    public int appFgRatio;
    public int appBgRatio;
    public int appFgSrvRatio;

    public int devChargingRatio;
    public int devUnChargingRatio;
    public int devSceneOffRatio;
    public int devLowEnergyRatio;

    public String sceneTop1;
    public int sceneTop1Ratio;
    public String sceneTop2;
    public int sceneTop2Ratio;

    public boolean isValid;
    public long duringMillis;

    @Nullable  private AtomicBoolean mForegroundOverride;

    AppStats() {
        sceneTop1 = "";
        sceneTop2 = "";
        isValid = false;
    }

    public long getMinute() {
        return Math.max(1, duringMillis / ONE_MIN);
    }

    public boolean isForeground() {
        if (mForegroundOverride != null) {
            return mForegroundOverride.get();
        }
        return getAppStat() == APP_STAT_FOREGROUND;
    }

    public boolean isCharging() {
        return getDevStat() == DEV_STAT_CHARGING;
    }

    public int getAppStat() {
        if (appFgRatio >= 50) return APP_STAT_FOREGROUND;
        if (appFgSrvRatio >= 50) return APP_STAT_FOREGROUND_SERVICE;
        return APP_STAT_BACKGROUND;
    }

    public AppStats setForeground(boolean bool) {
        mForegroundOverride = new AtomicBoolean(bool);
        return this;
    }

    public int getDevStat() {
        if (devChargingRatio >= 50) return DEV_STAT_CHARGING;
        if (devSceneOffRatio >= 50) return DEV_STAT_SCREEN_OFF;
        if (devLowEnergyRatio >= 50) return DEV_STAT_SAVE_POWER_MODE;
        return DEV_STAT_UN_CHARGING;
    }

    @Override
    public String toString() {
        return "AppStats{" +
                "appFgRatio=" + appFgRatio +
                ", appBgRatio=" + appBgRatio +
                ", appFgSrvRatio=" + appFgSrvRatio +
                ", devChargingRatio=" + devChargingRatio +
                ", devUnChargingRatio=" + devUnChargingRatio +
                ", devSceneOffRatio=" + devSceneOffRatio +
                ", devLowEnergyRatio=" + devLowEnergyRatio +
                ", sceneTop1='" + sceneTop1 + '\'' +
                ", sceneTop1Ratio=" + sceneTop1Ratio +
                ", sceneTop2='" + sceneTop2 + '\'' +
                ", sceneTop2Ratio=" + sceneTop2Ratio +
                ", isValid=" + isValid +
                ", duringMillis=" + duringMillis +
                ", foregroundOverride=" + mForegroundOverride +
                '}';
    }

    public static AppStats current() {
        return current(0L);
    }

    public static AppStats current(long millisFromNow) {
        long duringMillis = millisFromNow > 0 ? millisFromNow : 0L;

        AppStats stats = new AppStats();
        stats.duringMillis = duringMillis;
        AppStatMonitorFeature appStatFeat = BatteryCanary.getMonitorFeature(AppStatMonitorFeature.class);
        if (appStatFeat != null) {

            // configure appStat & scene
            AppStatMonitorFeature.AppStatSnapshot appStats = appStatFeat.currentAppStatSnapshot(duringMillis);
            if (appStats.isValid()) {
                stats.appFgRatio = appStats.fgRatio.get().intValue();
                stats.appBgRatio = appStats.bgRatio.get().intValue();
                stats.appFgSrvRatio = appStats.fgSrvRatio.get().intValue();

                TimeBreaker.TimePortions portions = appStatFeat.currentSceneSnapshot(duringMillis);
                Pair<String, Integer> top1 = portions.top1();
                if (top1 != null) {
                    stats.sceneTop1 = top1.first;
                    stats.sceneTop1Ratio = top1.second == null ? 0 : top1.second;
                    Pair<String, Integer> top2 = portions.top2();
                    if (top2 != null) {
                        stats.sceneTop2 = top2.first;
                        stats.sceneTop2Ratio = top2.second == null ? 0 : top2.second;
                    }

                    DeviceStatMonitorFeature devStatFeat = BatteryCanary.getMonitorFeature(DeviceStatMonitorFeature.class);
                    if (devStatFeat != null) {
                        // configure devStat
                        DeviceStatMonitorFeature.DevStatSnapshot devStat = devStatFeat.currentDevStatSnapshot(duringMillis);
                        if (devStat.isValid()) {
                            stats.devChargingRatio = devStat.chargingRatio.get().intValue();
                            stats.devUnChargingRatio = devStat.unChargingRatio.get().intValue();
                            stats.devSceneOffRatio = devStat.screenOff.get().intValue();
                            stats.devLowEnergyRatio = devStat.lowEnergyRatio.get().intValue();
                            stats.isValid = true;
                        }
                    }
                }
            }
        }
        return stats;
    }
}
