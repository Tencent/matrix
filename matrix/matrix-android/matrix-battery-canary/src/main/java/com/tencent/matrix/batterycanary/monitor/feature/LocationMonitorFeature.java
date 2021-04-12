package com.tencent.matrix.batterycanary.monitor.feature;

import androidx.annotation.NonNull;
import android.text.TextUtils;

import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.LocationManagerServiceHooker;
import com.tencent.matrix.util.MatrixLog;

public final class LocationMonitorFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.LocationMonitorFeature";
    final LocationTracing mTracing = new LocationTracing();
    LocationManagerServiceHooker.IListener mListener;

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public void onTurnOn() {
        super.onTurnOn();
        if (mCore.getConfig().isAmsHookEnabled) {
            mListener = new LocationManagerServiceHooker.IListener() {
                @Override
                public void onRequestLocationUpdates(long minTimeMillis, float minDistance) {
                    String stack = shouldTracing() ? BatteryCanaryUtil.stackTraceToString(new Throwable().getStackTrace()) : "";
                    MatrixLog.i(TAG, "#onRequestLocationUpdates, time = " + minTimeMillis + ", distance = " + minDistance + ", stack = " + stack);
                    mTracing.setStack(stack);
                    mTracing.onStartScan();
                }
            };
            LocationManagerServiceHooker.addListener(mListener);
        }
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        LocationManagerServiceHooker.removeListener(mListener);
        mTracing.onClear();
    }

    @Override
    public int weight() {
        return Integer.MIN_VALUE;
    }

    @NonNull
    public LocationTracing getTracing() {
        return mTracing;
    }

    public LocationSnapshot currentSnapshot() {
        return mTracing.getSnapshot();
    }

    public static final class LocationTracing {
        private int mScanCount;
        private String mLastConfiguredStack = "";

        public void setStack(String stack) {
            if (!TextUtils.isEmpty(stack)) {
                mLastConfiguredStack = stack;
            }
        }

        public void onStartScan() {
            mScanCount++;
        }

        public void onClear() {
            mScanCount = 0;
        }

        public LocationSnapshot getSnapshot() {
            LocationSnapshot snapshot = new LocationSnapshot();
            snapshot.scanCount = Snapshot.Entry.DigitEntry.of(mScanCount);
            snapshot.stack = mLastConfiguredStack;
            return snapshot;
        }
    }

    public static class LocationSnapshot extends Snapshot<LocationSnapshot> {
        public Entry.DigitEntry<Integer> scanCount;
        public String stack;

        @Override
        public Delta<LocationSnapshot> diff(LocationSnapshot bgn) {
            return new Delta<LocationSnapshot>(bgn, this) {
                @Override
                protected LocationSnapshot computeDelta() {
                    LocationSnapshot snapshot = new LocationSnapshot();
                    snapshot.scanCount = Differ.DigitDiffer.globalDiff(bgn.scanCount, end.scanCount);
                    snapshot.stack = end.stack;
                    return snapshot;
                }
            };
        }
    }
}
