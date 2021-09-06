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

package com.tencent.matrix.hook.memory;

import android.text.TextUtils;

import androidx.annotation.Keep;
import androidx.annotation.Nullable;

import com.tencent.matrix.hook.AbsHook;
import com.tencent.matrix.hook.HookManager;
import com.tencent.matrix.util.MatrixLog;

import java.util.HashSet;
import java.util.Set;

/**
 * Created by Yves on 2019-08-08
 */
public class MemoryHook extends AbsHook {

    private static final String TAG = "Matrix.MemoryHook";

    public static final MemoryHook INSTANCE = new MemoryHook();

    private final Set<String> mHookSoSet   = new HashSet<>();
    private final Set<String> mIgnoreSoSet = new HashSet<>();

    private int     mMinTraceSize;
    private int     mMaxTraceSize;
    private int     mStacktraceLogThreshold = 10 * 1024 * 1024;
    private boolean mEnableStacktrace;
    private boolean mEnableMmap;
    private boolean mHookInstalled = false;

    private MemoryHook() {
    }

    public MemoryHook addHookSo(String regex) {
        if (TextUtils.isEmpty(regex)) {
            MatrixLog.e(TAG, "thread regex is empty!!!");
        } else {
            mHookSoSet.add(regex);
        }
        return this;
    }

    public MemoryHook addHookSo(String... regexArr) {
        for (String regex : regexArr) {
            addHookSo(regex);
        }
        return this;
    }

    public MemoryHook addIgnoreSo(String regex) {
        if (TextUtils.isEmpty(regex)) {
            return this;
        }
        mIgnoreSoSet.add(regex);
        return this;
    }

    public MemoryHook addIgnoreSo(String... regexArr) {
        for (String regex : regexArr) {
            addIgnoreSo(regex);
        }
        return this;
    }

    public MemoryHook enableStacktrace(boolean enable) {
        mEnableStacktrace = enable;
        return this;
    }

    /**
     * >= 0, 0 表示不限制
     *
     * @param size
     * @return
     */
    public MemoryHook tracingAllocSizeRange(int min, int max) {
        mMinTraceSize = min;
        mMaxTraceSize = max;
        return this;
    }

    public MemoryHook enableMmapHook(boolean enable) {
        mEnableMmap = enable;
        return this;
    }

    public MemoryHook stacktraceLogThreshold(int threshold) {
        mStacktraceLogThreshold = threshold;
        return this;
    }

    /**
     * notice: it is an exclusive interface
     */
    public void hook() throws HookManager.HookFailedException {
        HookManager.INSTANCE
                .clearHooks()
                .addHook(this)
                .commitHooks();
    }

    @Nullable
    @Override
    protected String getNativeLibraryName() {
        return "matrix-memoryhook";
    }

    @Override
    public boolean onConfigure() {
        if (mMinTraceSize < 0 || (mMaxTraceSize != 0 && mMaxTraceSize < mMinTraceSize)) {
            throw new IllegalArgumentException("sizes should not be negative and maxSize should be " +
                    "0 or greater than minSize: min = " + mMinTraceSize + ", max = " + mMaxTraceSize);
        }

        MatrixLog.d(TAG, "enable mmap? " + mEnableMmap);
        enableMmapHookNative(mEnableMmap);

        setTracingAllocSizeRangeNative(mMinTraceSize, mMaxTraceSize);
        setStacktraceLogThresholdNative(mStacktraceLogThreshold);
        enableStacktraceNative(mEnableStacktrace);

        return true;
    }

    @Override
    protected boolean onHook(boolean enableDebug) {
        if (!mHookInstalled) {
            installHooksNative(mHookSoSet.toArray(new String[0]), mIgnoreSoSet.toArray(new String[0]), enableDebug);
            mHookInstalled = true;
        }
        return true;
    }

    public void dump(String logPath, String jsonPath) {
        if (getStatus() == Status.COMMIT_SUCCESS) {
            dumpNative(logPath, jsonPath);
        }
    }

    @Keep
    private native void dumpNative(String logPath, String jsonPath);

    @Keep
    private native void setTracingAllocSizeRangeNative(int minSize, int maxSize);

    @Keep
    private native void enableStacktraceNative(boolean enable);

    @Keep
    private native void enableMmapHookNative(boolean enable);

    @Keep
    private native void setStacktraceLogThresholdNative(int threshold);

    @Keep
    private native void installHooksNative(String[] hookSoPatterns, String[] ignoreSoPatterns, boolean enableDebug);
}

