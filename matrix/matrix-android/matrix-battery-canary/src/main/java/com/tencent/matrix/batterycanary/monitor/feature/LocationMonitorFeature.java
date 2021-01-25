package com.tencent.matrix.batterycanary.monitor.feature;

import com.tencent.matrix.batterycanary.utils.LocationManagerServiceHooker;
import com.tencent.matrix.util.MatrixLog;

public final class LocationMonitorFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.LocationMonitorFeature";
    final LocationCounting mCounting = new LocationCounting();
    LocationManagerServiceHooker.IListener mListener;

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public void onTurnOn() {
        super.onTurnOn();
        mListener = new LocationManagerServiceHooker.IListener() {
            @Override
            public void onRequestLocationUpdates(long minTimeMillis, float minDistance) {
                MatrixLog.i(TAG, "#onRequestLocationUpdates, time = " + minTimeMillis + ", distance = " + minDistance);
                mCounting.onStartScan();
            }
        };
        LocationManagerServiceHooker.addListener(mListener);
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        LocationManagerServiceHooker.removeListener(mListener);
        mCounting.onClear();
    }

    @Override
    public int weight() {
        return Integer.MIN_VALUE;
    }

    public LocationSnapshot currentSnapshot() {
        return mCounting.getSnapshot();
    }

    public static final class LocationCounting {
        private int mScanCount;

        public void onStartScan() {
            mScanCount++;
        }

        public void onClear() {
            mScanCount = 0;
        }

        public LocationSnapshot getSnapshot() {
            LocationSnapshot snapshot = new LocationSnapshot();
            snapshot.scanCount = Snapshot.Entry.DigitEntry.of(mScanCount);
            snapshot.stack = "";
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
                    return snapshot;
                }
            };
        }
    }
}
