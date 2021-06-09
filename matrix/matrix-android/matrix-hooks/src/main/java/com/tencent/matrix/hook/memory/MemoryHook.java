package com.tencent.matrix.hook.memory;

import android.text.TextUtils;


import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.hook.AbsHook;
import com.tencent.matrix.hook.HookManager;

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
     *
     * @param min >= 0, 0 表示不限制
     * @param max 0 或 > minSize, 0 表示不限制
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

    @Override
    public void onConfigure() {
        if (mMinTraceSize < 0 || (mMaxTraceSize != 0 && mMaxTraceSize < mMinTraceSize)) {
            throw new IllegalArgumentException("sizes should not be negative and maxSize should be " +
                    "0 or greater than minSize: min = " + mMinTraceSize + ", max = " + mMaxTraceSize);
        }

        MatrixLog.d(TAG, "enable mmap? " + mEnableMmap);
        enableMmapHookNative(mEnableMmap);

        setTracingAllocSizeRangeNative(mMinTraceSize, mMaxTraceSize);
        setStacktraceLogThresholdNative(mStacktraceLogThreshold);
        enableStacktraceNative(mEnableStacktrace);
    }

    @Override
    protected void onHook() {
        addHookSoNative(mHookSoSet.toArray(new String[0]));
        addIgnoreSoNative(mIgnoreSoSet.toArray(new String[0]));
    }

    public void dump(String logPath, String jsonPath) {
        if (HookManager.INSTANCE.hasHooked()) {
            dumpNative(logPath, jsonPath);
        }
    }

    private native void dumpNative(String logPath, String jsonPath);

    private native void setTracingAllocSizeRangeNative(int minSize, int maxSize);

    private native void enableStacktraceNative(boolean enable);

    private native void enableMmapHookNative(boolean enable);

    private native void addHookSoNative(String[] hookSoList);

    private native void addIgnoreSoNative(String[] ignoreSoList);

    private native void setStacktraceLogThresholdNative(int threshold);
}

