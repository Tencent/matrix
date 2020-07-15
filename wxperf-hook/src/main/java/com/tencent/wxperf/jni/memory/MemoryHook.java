package com.tencent.wxperf.jni.memory;

import android.text.TextUtils;
import android.util.Log;

import com.tencent.wxperf.jni.AbsHook;
import com.tencent.wxperf.jni.HookManager;

import java.util.HashSet;
import java.util.Set;

/**
 * Created by Yves on 2019-08-08
 */
public class MemoryHook extends AbsHook {

    public static final MemoryHook INSTANCE = new MemoryHook();

    private Set<String> mHookSoSet   = new HashSet<>();
    private Set<String> mIgnoreSoSet = new HashSet<>();

    private int     mMinTraceSize;
    private int     mMaxTraceSize;
    private double  mSampling = 1;
    private boolean mEnableStacktrace;
    private boolean mEnableMmap;

    private MemoryHook() {
    }

    public MemoryHook addHookSo(String regex) {
        if (TextUtils.isEmpty(regex)) {
            throw new IllegalArgumentException("regex = " + regex);
        }
        mHookSoSet.add(regex);
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
    public MemoryHook minTraceSize(int size) {
        mMinTraceSize = size;
        return this;
    }

    /**
     * 0 或 > minSize, 0 表示不限制
     *
     * @param size
     * @return
     */
    public MemoryHook maxTraceSize(int size) {
        mMaxTraceSize = size;
        return this;
    }

    /**
     * [0,1]
     *
     * @param sampling
     * @return
     */
    public MemoryHook sampling(double sampling) {
        if (mSampling < 0 || mSampling > 1) {
            throw new IllegalArgumentException("sampling should be between 0 and 1: " + sampling);
        }
        mSampling = sampling;
        return this;
    }

    public MemoryHook enableMmapHook(boolean enable) {
        mEnableMmap = enable;
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

        Log.d("Yves.debug", "enable mmap? " + mEnableMmap);
        enableMmapHookNative(mEnableMmap);

        setSampleSizeRangeNative(mMinTraceSize, mMaxTraceSize);
        setSamplingNative(mSampling);

        enableStacktraceNative(mEnableStacktrace);
    }

    @Override
    protected void onHook() {
        addHookSoNative(mHookSoSet.toArray(new String[0]));
        addIgnoreSoNative(mIgnoreSoSet.toArray(new String[0]));
    }

    public void dump(String into) {
        dumpNative(into);
    }

    private native void dumpNative(String path);

    private native void setSamplingNative(double sampling);

    private native void setSampleSizeRangeNative(int minSize, int maxSize);

    private native void enableStacktraceNative(boolean enable);

    private native void enableMmapHookNative(boolean enable);

    private native void addHookSoNative(String[] hookSoList);

    private native void addIgnoreSoNative(String[] ignoreSoList);
}

