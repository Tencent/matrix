package com.tencent.matrix.hook.memory;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;

import androidx.annotation.Nullable;

import com.tencent.matrix.hook.HookManager.NativeLibraryLoader;
import com.tencent.matrix.util.MatrixLog;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.util.concurrent.TimeUnit;
import java.util.regex.Pattern;

public final class GCSemiSpaceTrimmer {
    private static final String TAG = "Matrix.GCSemiSpaceTrimmer";

    public static final GCSemiSpaceTrimmer INSTANCE = new GCSemiSpaceTrimmer();

    private static final Pattern NOT_NUM_PATTERN = Pattern.compile("[^0-9]");
    private static final long DEFAULT_VMSIZE_SAMPLE_INTERVAL = TimeUnit.MINUTES.toMillis(3);

    private NativeLibraryLoader mNativeLibLoader = null;
    private float mCriticalVmSizeRatio = 0.0f;
    private long mVmSizeSampleInterval = DEFAULT_VMSIZE_SAMPLE_INTERVAL;
    private HandlerThread mSampleThread = null;
    private Handler mSampleHandler = null;
    private boolean mNativeLibLoaded = false;
    private boolean mInstalled = false;

    public GCSemiSpaceTrimmer setNativeLibraryLoader(@Nullable NativeLibraryLoader loader) {
        mNativeLibLoader = loader;
        return this;
    }

    private boolean ensureNativeLibLoaded() {
        synchronized (this) {
            if (mNativeLibLoaded) {
                return true;
            }
            try {
                if (mNativeLibLoader != null) {
                    mNativeLibLoader.loadLibrary("matrix-hookcommon");
                    mNativeLibLoader.loadLibrary("matrix-memoryhook");
                } else {
                    System.loadLibrary("matrix-hookcommon");
                    System.loadLibrary("matrix-memoryhook");
                }
                mNativeLibLoaded = true;
            } catch (Throwable thr) {
                MatrixLog.printErrStackTrace(TAG, thr, "Fail to load native library.");
                mNativeLibLoaded = false;
            }
            return mNativeLibLoaded;
        }
    }

    public boolean isCompatible() {
        synchronized (this) {
            if (!ensureNativeLibLoaded()) {
                return false;
            }
            return nativeIsCompatible();
        }
    }

    public boolean install(float criticalVmSizeRatio, long vmsizeSampleInterval, @Nullable Looper vmSampleLooper) {
        synchronized (this) {
            if (mInstalled) {
                MatrixLog.e(TAG, "Already installed.");
                return true;
            }
            if (!ensureNativeLibLoaded()) {
                MatrixLog.e(TAG, "Fail to load native library.");
                return false;
            }
            mCriticalVmSizeRatio = criticalVmSizeRatio;
            if (vmsizeSampleInterval > 0) {
                mVmSizeSampleInterval = vmsizeSampleInterval;
            } else if (vmsizeSampleInterval == 0) {
                mVmSizeSampleInterval = DEFAULT_VMSIZE_SAMPLE_INTERVAL;
            } else {
                MatrixLog.e(TAG, "vmsizeSampleInterval cannot less than zero. (value: " + vmsizeSampleInterval + ")");
                return false;
            }
            if (vmSampleLooper != null) {
                mSampleHandler = new Handler(vmSampleLooper);
            } else {
                mSampleThread = new HandlerThread("Matrix.GCSST");
                mSampleThread.start();
                mSampleHandler = new Handler(mSampleThread.getLooper());
            }
            mSampleHandler.postDelayed(mSampleTask, mVmSizeSampleInterval);
            MatrixLog.i(TAG, "Installed, critcal_vmsize_ratio: %s, vmsize_sample_interval: %s",
                    criticalVmSizeRatio, vmsizeSampleInterval);
            return true;
        }
    }

    private static long readVmSize() {
        long vssSize = -1L;
        try (BufferedReader br = new BufferedReader(new InputStreamReader(new FileInputStream("/proc/self/status")))) {
            String content;
            while ((content = br.readLine()) != null) {
                content = content.toLowerCase();
                if (content.contains("vmsize")) {
                    // current vss size
                    vssSize = Long.parseLong(NOT_NUM_PATTERN.matcher(content).replaceAll("").trim()) * 1024;
                    break;
                }
            }
        } catch (Throwable thr) {
            MatrixLog.printErrStackTrace(TAG, thr, "read proc status failed.");
        }
        return vssSize;
    }

    private final Runnable mSampleTask = new Runnable() {
        @Override
        public void run() {
            final long vmSize = readVmSize();
            if (vmSize < 0) {
                MatrixLog.e(TAG, "Fail to read vss size, skip checking this time.");
                mSampleHandler.postDelayed(this, mVmSizeSampleInterval);
            } else {
                // (vmsize / 4G > ratio) => (vmsize > ratio * 4G)
                if (vmSize > 4L * 1024 * 1024 * 1024 * mCriticalVmSizeRatio) {
                    MatrixLog.i(TAG, "VmSize usage reaches above critical level, trigger native install."
                            + " vmsize: %s, critical_ratio: %s", vmSize, mCriticalVmSizeRatio);
                    final boolean nativeInstallRes = nativeInstall();
                    if (nativeInstallRes) {
                        MatrixLog.i(TAG, "nativeInstall triggered successfully.");
                    } else {
                        MatrixLog.i(TAG, "Fail to trigger nativeInstall.");
                    }
                } else {
                    MatrixLog.i(TAG, "VmSize usage is under critical level, check next time."
                            + " vmsize: %s, critical_ratio: %s", vmSize, mCriticalVmSizeRatio);
                    mSampleHandler.postDelayed(this, mVmSizeSampleInterval);
                }
            }
        }
    };

    private native boolean nativeIsCompatible();
    private native boolean nativeInstall();

    private GCSemiSpaceTrimmer() {
        // Do nothing.
    }
}
