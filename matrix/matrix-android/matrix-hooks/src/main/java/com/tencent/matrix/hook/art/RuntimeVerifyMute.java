package com.tencent.matrix.hook.art;

import androidx.annotation.Nullable;

import com.tencent.matrix.hook.HookManager.NativeLibraryLoader;
import com.tencent.matrix.util.MatrixLog;

/**
 * Created by tomystang on 2022/11/16.
 */
public final class RuntimeVerifyMute {
    private static final String TAG = "Matrix.RuntimeVerifyMute";

    public static final RuntimeVerifyMute INSTANCE = new RuntimeVerifyMute();

    private NativeLibraryLoader mNativeLibLoader = null;
    private boolean mNativeLibLoaded = false;

    public RuntimeVerifyMute setNativeLibraryLoader(@Nullable NativeLibraryLoader loader) {
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
                    mNativeLibLoader.loadLibrary("matrix-artmisc");
                } else {
                    System.loadLibrary("matrix-hookcommon");
                    System.loadLibrary("matrix-artmisc");
                }
                mNativeLibLoaded = true;
            } catch (Throwable thr) {
                MatrixLog.printErrStackTrace(TAG, thr, "Fail to load native library.");
                mNativeLibLoaded = false;
            }
            return mNativeLibLoaded;
        }
    }

    public boolean install() {
        if (!ensureNativeLibLoaded()) {
            return false;
        }
        return nativeInstall();
    }

    private static native boolean nativeInstall();

    private RuntimeVerifyMute() { }
}
