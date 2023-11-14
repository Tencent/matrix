package com.tencent.matrix.batterycanary.monitor.feature;

import android.bluetooth.le.ScanSettings;
import android.os.Build;
import android.text.TextUtils;

import com.tencent.matrix.batterycanary.utils.BluetoothManagerServiceHooker;
import com.tencent.matrix.util.MatrixLog;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import static com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig.AMS_HOOK_FLAG_BT;

public final class BlueToothMonitorFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.BlueToothMonitorFeature";
    final BlueToothTracing mTracing = new BlueToothTracing();
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
        if (mCore.getConfig().isAmsHookEnabled
                || ((mCore.getConfig().amsHookEnableFlag & AMS_HOOK_FLAG_BT) == AMS_HOOK_FLAG_BT)) {

            mListener = new BluetoothManagerServiceHooker.IListener() {
                @Override
                public void onRegisterScanner() {
                    String stack = shouldTracing() ? mCore.getConfig().callStackCollector.collectCurr() : "";
                    MatrixLog.i(TAG, "#onRegisterScanner, stack = " + stack);
                    mTracing.setStack(stack);
                    mTracing.onRegisterScanner();
                }

                @Override
                public void onStartDiscovery() {
                    String stack = shouldTracing() ? mCore.getConfig().callStackCollector.collectCurr() : "";
                    MatrixLog.i(TAG, "#onStartDiscovery, stack = " + stack);
                    mTracing.setStack(stack);
                    mTracing.onStartDiscovery();
                }

                @Override
                public void onStartScan(int scanId, @Nullable ScanSettings scanSettings) {
                    // callback from H handler
                    MatrixLog.i(TAG, "#onStartScan, id = " + scanId);
                    mTracing.onStartScan();
                }

                @Override
                public void onStartScanForIntent(@Nullable ScanSettings scanSettings) {
                    // callback from H handler
                    MatrixLog.i(TAG, "#onStartScanForIntent");
                    mTracing.onStartScan();
                }
            };
            BluetoothManagerServiceHooker.addListener(mListener);
        }
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        BluetoothManagerServiceHooker.removeListener(mListener);
        mTracing.onClear();
    }

    @Override
    public int weight() {
        return Integer.MIN_VALUE;
    }

    @NonNull
    public BlueToothTracing getTracing() {
        return mTracing;
    }

    public BlueToothSnapshot currentSnapshot() {
        return mTracing.getSnapshot();
    }

    public static final class BlueToothTracing {
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
                    snapshot.stack = end.stack;
                    return snapshot;
                }
            };
        }
    }
}
