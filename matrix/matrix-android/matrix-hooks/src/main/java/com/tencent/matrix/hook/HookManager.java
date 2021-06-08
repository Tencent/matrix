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


import androidx.annotation.Keep;
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

    private volatile boolean hasHooked;
    private Set<AbsHook> mHooks = new HashSet<>();

    private HookManager(){
    }

    private void exclusiveHook() {
        xhookEnableDebugNative(BuildConfig.DEBUG);
        xhookEnableSigSegvProtectionNative(!BuildConfig.DEBUG);

        xhookRefreshNative(false);

        hasHooked = true;
    }

    public void commitHooks() throws HookFailedException {
        if (hasHooked) {
            throw new HookFailedException("this process has already been hooked!");
        }

        if (mHooks.isEmpty()) {
            return;
        }

        try {
            System.loadLibrary("matrix-hooks");
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
            return;
        }

        for (AbsHook hook : mHooks) {
            hook.onConfigure();
        }
        for (AbsHook hook : mHooks) {
            hook.onHook();
        }
        exclusiveHook();
    }

    public HookManager addHook(@Nullable AbsHook hook) {
        if (hook != null) {
            mHooks.add(hook);
        }
        return this;
    }

    public HookManager clearHooks() {
        mHooks.clear();
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

    private native int xhookRefreshNative(boolean async);
    private native void xhookEnableDebugNative(boolean flag);
    private native void xhookEnableSigSegvProtectionNative(boolean flag);
    private native void xhookClearNative();

    public static class HookFailedException extends Exception {

        public HookFailedException(String message) {
            super(message);
        }
    }
}
