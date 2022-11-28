/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

    private volatile boolean mHasNativeInitialized = false;
    private byte[] mInitializeGuard = {};
    private final Set<AbsHook> mPendingHooks = new HashSet<>();
    private volatile boolean mEnableDebug = BuildConfig.DEBUG;

    private NativeLibraryLoader mNativeLibLoader = null;

    public interface NativeLibraryLoader {
        void loadLibrary(@NonNull String libName);
    }

    private HookManager() {
        // Do nothing.
    }

    public void commitHooks() throws HookFailedException {
        synchronized (mInitializeGuard) {
            synchronized (mPendingHooks) {
                if (mPendingHooks.isEmpty()) {
                    return;
                }
            }
            if (!mHasNativeInitialized) {
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

                if (!doPreHookInitializeNative(mEnableDebug)) {
                    throw new HookFailedException("Fail to do hook common pre-hook initialize.");
                }

                commitHooksLocked();

                doFinalInitializeNative(mEnableDebug);
                mHasNativeInitialized = true;
            } else {
                commitHooksLocked();
            }
        }
    }

    private void commitHooksLocked() throws HookFailedException {
        synchronized (mPendingHooks) {
            for (AbsHook hook : mPendingHooks) {
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
                    hook.setStatus(AbsHook.Status.COMMIT_FAIL_ON_LOAD_LIB);
                }
            }
            for (AbsHook hook : mPendingHooks) {
                if (hook.getStatus() != AbsHook.Status.UNCOMMIT) {
                    MatrixLog.e(TAG, "%s has failed steps before, skip calling onConfigure on it.",
                            hook.getClass().getName());
                    continue;
                }
                if (!hook.onConfigure()) {
                    MatrixLog.e(TAG, "Fail to configure %s, skip next steps", hook.getClass().getName());
                    hook.setStatus(AbsHook.Status.COMMIT_FAIL_ON_CONFIGURE);
                }
            }
            for (AbsHook hook : mPendingHooks) {
                if (hook.getStatus() != AbsHook.Status.UNCOMMIT) {
                    MatrixLog.e(TAG, "%s has failed steps before, skip calling onHook on it.",
                            hook.getClass().getName());
                    continue;
                }
                if (hook.onHook(mEnableDebug)) {
                    MatrixLog.i(TAG, "%s is committed successfully.", hook.getClass().getName());
                    hook.setStatus(AbsHook.Status.COMMIT_SUCCESS);
                } else {
                    MatrixLog.e(TAG, "Fail to do hook in %s.", hook.getClass().getName());
                    hook.setStatus(AbsHook.Status.COMMIT_FAIL_ON_HOOK);
                }
            }
            mPendingHooks.clear();
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
        if (hook != null && hook.getStatus() != AbsHook.Status.COMMIT_SUCCESS) {
            synchronized (mPendingHooks) {
                mPendingHooks.add(hook);
            }
        }
        return this;
    }

    public HookManager clearHooks() {
        synchronized (mPendingHooks) {
            mPendingHooks.clear();
            return this;
        }
    }

    @Keep
    public static String getStack() {
        try {
            return stackTraceToString(Thread.currentThread().getStackTrace());
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
            return "";
        }
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

    private native boolean doPreHookInitializeNative(boolean debug);

    private native void doFinalInitializeNative(boolean debug);

    public static class HookFailedException extends Exception {
        public HookFailedException(String message) {
            super(message);
        }
    }
}
