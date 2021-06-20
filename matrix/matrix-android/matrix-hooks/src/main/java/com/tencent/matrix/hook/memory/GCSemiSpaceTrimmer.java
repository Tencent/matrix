package com.tencent.matrix.hook.memory;

import androidx.annotation.Nullable;

import com.tencent.matrix.hook.HookManager.NativeLibraryLoader;
import com.tencent.matrix.util.MatrixLog;

public final class GCSemiSpaceTrimmer {
    private static final String TAG = "Matrix.GCSemiSpaceTrimmer";

    public static final GCSemiSpaceTrimmer INSTANCE = new GCSemiSpaceTrimmer();

    private NativeLibraryLoader mNativeLibLoader = null;
    private boolean mNativeLibLoaded = false;
    private boolean mInstalled = false;

    public GCSemiSpaceTrimmer setNativeLibraryLoader(@Nullable NativeLibraryLoader loader) {
        mNativeLibLoader = loader;
        return this;
    }

    public boolean install() {
        synchronized (this) {
            if (mInstalled) {
                MatrixLog.e(TAG, "Alreay installed.");
                return true;
            }
            if (!mNativeLibLoaded) {
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
            }
            if (!mNativeLibLoaded) {
                return false;
            }
            mInstalled = nativeInstall();
            return mInstalled;
        }
    }

    private native boolean nativeInstall();

    private GCSemiSpaceTrimmer() {
        // Do nothing.
    }
}
