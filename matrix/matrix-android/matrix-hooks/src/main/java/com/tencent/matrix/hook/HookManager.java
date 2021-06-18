package com.tencent.matrix.hook;


import android.text.TextUtils;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.tencent.matrix.util.MatrixLog;

import java.util.HashSet;
import java.util.Set;

/**
 * Created by Yves on 2020-03-17
 */
public class HookManager {
    private static final String TAG = "Matrix.HookManager";

    public static final HookManager INSTANCE = new HookManager();

    private volatile boolean hasHooked = false;
    private final Set<AbsHook> mPendingHooks = new HashSet<>();
    private final Set<AbsHook> mCommitedHooks = new HashSet<>();
    private volatile boolean mEnableDebug = BuildConfig.DEBUG;

    private NativeLibraryLoader mNativeLibLoader = null;

    public interface NativeLibraryLoader {
        void loadLibrary(@NonNull String libName);
    }

    private HookManager() {
        // Do nothing.
    }

    public void commitHooks() throws HookFailedException {
        if (mPendingHooks.isEmpty()) {
            return;
        }

        if (!hasHooked()) {
            try {
                if (mNativeLibLoader != null) {
                    mNativeLibLoader.loadLibrary("matrix-hookcommon");
                } else {
                    System.loadLibrary("matrix-hookcommon");
                }
            } catch (Throwable e) {
                MatrixLog.printErrStackTrace(TAG, e, "");
                return;
            }

            if (!doPreHookInitializeNative()) {
                throw new HookFailedException("Fail to do hook common pre-hook initialize.");
            }
        }

        final Set<AbsHook> failureHooks = new HashSet<>();
        for (AbsHook hook : mPendingHooks) {
            if (mCommitedHooks.contains(hook)) {
                continue;
            }
            final String nativeLibName = hook.getNativeLibraryName();
            if (TextUtils.isEmpty(nativeLibName)) {
                continue;
            }
            try {
                if (mNativeLibLoader != null) {
                    mNativeLibLoader.loadLibrary(nativeLibName);
                } else {
                    System.loadLibrary(nativeLibName);
                }
            } catch (Throwable e) {
                MatrixLog.printErrStackTrace(TAG, e, "");
                MatrixLog.e(TAG, "Fail to load native library for %s, skip next steps.",
                        hook.getClass().getName());
                failureHooks.add(hook);
            }
        }
        for (AbsHook hook : mPendingHooks) {
            if (failureHooks.contains(hook)) {
                MatrixLog.e(TAG, "%s has failed steps before, skip calling onConfigure on it.",
                        hook.getClass().getName());
                continue;
            }
            hook.onConfigure();
        }
        for (AbsHook hook : mPendingHooks) {
            if (failureHooks.contains(hook)) {
                MatrixLog.e(TAG, "%s has failed steps before, skip calling onHook on it.",
                        hook.getClass().getName());
                continue;
            }
            hook.onHook(mEnableDebug);
        }

        mPendingHooks.removeAll(failureHooks);
        mCommitedHooks.addAll(mPendingHooks);
        mPendingHooks.clear();

        if (!hasHooked()) {
            doFinalInitializeNative();
            hasHooked = true;
        }
    }

    public HookManager setEnableDebug(boolean enabled) {
        mEnableDebug = enabled;
        return this;
    }

    public HookManager setNativeLibraryLoader(@Nullable NativeLibraryLoader loader) {
        mNativeLibLoader = loader;
        return this;
    }

    public HookManager addHook(@Nullable AbsHook hook) {
        if (hook != null) {
            mPendingHooks.add(hook);
        }
        return this;
    }

    public HookManager clearHooks() {
        mPendingHooks.clear();
        return this;
    }

    @Keep
    public static String getStack() {
        return stackTraceToString(Thread.currentThread().getStackTrace());
    }

    private static String stackTraceToString(final StackTraceElement[] arr) {
        if (arr == null) {
            return "";
        }

        StringBuilder sb = new StringBuilder();

        for (StackTraceElement stackTraceElement : arr) {
            String className = stackTraceElement.getClassName();
            // remove unused stacks
            if (className.contains("java.lang.Thread")) {
                continue;
            }

            sb.append(stackTraceElement).append(';');
        }
        return sb.toString();
    }

    public boolean hasHooked() {
        return hasHooked;
    }

    private native boolean doPreHookInitializeNative();
    private native void doFinalInitializeNative();

    public static class HookFailedException extends Exception {
        public HookFailedException(String message) {
            super(message);
        }
    }
}
