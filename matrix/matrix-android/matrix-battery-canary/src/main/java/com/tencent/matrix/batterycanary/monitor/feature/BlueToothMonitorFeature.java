package com.tencent.matrix.batterycanary.monitor.feature;

import android.bluetooth.le.ScanSettings;
import android.os.Build;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.BluetoothManagerServiceHooker;
import com.tencent.matrix.util.MatrixLog;

public final class BlueToothMonitorFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.BlueToothMonitorFeature";
    final BlueToothCounting mCounting = new BlueToothCounting();
    BluetoothManagerServiceHooker.IListener mListener;

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public void onTurnOn() {
        super.onTurnOn();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            MatrixLog.w(TAG, "only support >= android 8.0 for the moment");
            return;
        }
        mListener = new BluetoothManagerServiceHooker.IListener() {
            @Override
            public void onRegisterScanner() {
                String stack = shouldTracing() ? BatteryCanaryUtil.stackTraceToString(new Throwable().getStackTrace()) : "";
                MatrixLog.i(TAG, "#onRegisterScanner, stack = " + stack);
                mCounting.setStack(stack);
                mCounting.onRegisterScanner();
            }

            @Override
            public void onStartDiscovery() {
                String stack = shouldTracing() ? BatteryCanaryUtil.stackTraceToString(new Throwable().getStackTrace()) : "";
                MatrixLog.i(TAG, "#onStartDiscovery, stack = " + stack);
                mCounting.setStack(stack);
                mCounting.onStartDiscovery();
            }

            @Override
            public void onStartScan(int scanId, @Nullable ScanSettings scanSettings) {
                // callback from H handler
                MatrixLog.i(TAG, "#onStartScan, id = " + scanId);
                mCounting.onStartScan();
            }

            @Override
            public void onStartScanForIntent(@Nullable ScanSettings scanSettings) {
                // callback from H handler
                MatrixLog.i(TAG, "#onStartScanForIntent");
                mCounting.onStartScan();
            }
        };
        BluetoothManagerServiceHooker.addListener(mListener);
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        BluetoothManagerServiceHooker.removeListener(mListener);
        mCounting.onClear();
    }

    @Override
    public int weight() {
        return Integer.MIN_VALUE;
    }

    public BlueToothSnapshot currentSnapshot() {
        return mCounting.getSnapshot();
    }

    public static final class BlueToothCounting {
        private int mRegsCount;
        private int mDiscCount;
        private int mScanCount;
        private String mLastConfiguredStack = "";

        public void setStack(String stack) {
            if (!TextUtils.isEmpty(stack)) {
                mLastConfiguredStack = stack;
            }
        }

        public void onRegisterScanner() {
            mRegsCount++;
        }

        public void onStartDiscovery() {
            mDiscCount++;
        }

        public void onStartScan() {
            mScanCount++;
        }

        public void onClear() {
            mRegsCount = 0;
            mDiscCount = 0;
            mScanCount = 0;
        }

        public BlueToothSnapshot getSnapshot() {
            BlueToothSnapshot snapshot = new BlueToothSnapshot();
            snapshot.regsCount = Snapshot.Entry.DigitEntry.of(mRegsCount);
            snapshot.discCount = Snapshot.Entry.DigitEntry.of(mDiscCount);
            snapshot.scanCount = Snapshot.Entry.DigitEntry.of(mScanCount);
            snapshot.stack = mLastConfiguredStack;
            return snapshot;
        }
    }

    public static class BlueToothSnapshot extends Snapshot<BlueToothSnapshot> {
        public Entry.DigitEntry<Integer> regsCount;
        public Entry.DigitEntry<Integer> discCount;
        public Entry.DigitEntry<Integer> scanCount;
        public String stack;

        @Override
        public Delta<BlueToothSnapshot> diff(BlueToothSnapshot bgn) {
            return new Delta<BlueToothSnapshot>(bgn, this) {
                @Override
                protected BlueToothSnapshot computeDelta() {
                    BlueToothSnapshot snapshot = new BlueToothSnapshot();
                    snapshot.regsCount = Differ.DigitDiffer.globalDiff(bgn.regsCount, end.regsCount);
                    snapshot.discCount = Differ.DigitDiffer.globalDiff(bgn.discCount, end.discCount);
                    snapshot.scanCount = Differ.DigitDiffer.globalDiff(bgn.scanCount, end.scanCount);
                    return snapshot;
                }
            };
        }
    }
}
