package com.tencent.mm.performance.jni.memory;

import com.tencent.mm.performance.jni.BuildConfig;
import com.tencent.mm.performance.jni.LibWxPerfManager;

import java.util.HashSet;
import java.util.Set;

/**
 * Created by Yves on 2019-08-08
 */
public class MemoryHook {

//    public static final String HOOK_STATE_NONE = "none";
//    public static final String HOOK_STATE_HOOK = "hook";

    private Set<String> mHookSoSet   = new HashSet<>();
    private Set<String> mIgnoreSoSet = new HashSet<>();

    private int     mMinTraceSize;
    private int     mMaxTraceSize;
    private double  mSampling = 1;
    private boolean mEnableStacktrace;

    static {
        LibWxPerfManager.INSTANCE.init();
    }

    public static final MemoryHook INSTANCE = new MemoryHook();

    private MemoryHook() {
        xhookInitNative();
    }

    public MemoryHook addHookSo(String regex) {
        if (regex == null || regex.length() <= 0) {
            throw new IllegalArgumentException("regex = " + regex);
        }
        mHookSoSet.add(regex);
        return this;
    }

    public MemoryHook addHookSo(String... regexArr) {
        for (String s : regexArr) {
            if (s == null || s.length() <= 0) {
                throw new IllegalArgumentException("regex = " + s);
            }
            mHookSoSet.add(s);
        }
        return this;
    }

    public MemoryHook addIgnoreSo(String regex) {
        if (regex == null || regex.length() <= 0) {
            return this;
        }
        mIgnoreSoSet.add(regex);
        return this;
    }

    public MemoryHook addIgnoreSo(String... regexArr) {
        for (String s : regexArr) {
            if (s == null || s.length() <= 0) {
                continue;
            }
            mIgnoreSoSet.add(s);
        }
        return this;
    }

    public MemoryHook enableStacktrace(boolean enable) {
        mEnableStacktrace = enable;
        return this;
    }

    /**
     *  >= 0, 0 表示不限制
     * @param size
     * @return
     */
    public MemoryHook minTraceSize(int size) {
        mMinTraceSize = size;
        return this;
    }

    /**
     *  0 或 > minSize, 0 表示不限制
     * @param size
     * @return
     */
    public MemoryHook maxTraceSize(int size) {
        mMaxTraceSize = size;
        return this;
    }

    /**
     * [0,1]
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
        enableMmapHookNative(enable);
        return this;
    }

    public void hook() {
        xhookRegisterNative(mHookSoSet.toArray(new String[0]));
        xhookIgnoreNative(mIgnoreSoSet.toArray(new String[0]));

        if (mMinTraceSize < 0 || (mMaxTraceSize != 0 && mMaxTraceSize < mMinTraceSize)) {
            throw new IllegalArgumentException("sizes should not be negative and maxSize should be " +
                    "0 or greater than minSize: min = " + mMinTraceSize + ", max = " + mMaxTraceSize);
        }

        setSampleSizeRangeNative(mMinTraceSize, mMaxTraceSize);
        setSamplingNative(mSampling);

        enableStacktraceNative(mEnableStacktrace);
        xhookEnableDebugNative(BuildConfig.DEBUG);
        xhookEnableSigSegvProtectionNative(!BuildConfig.DEBUG);
        groupByMemorySize(true);
        xhookRefreshNative(false);
    }

    public void dump(String into) {
        dumpNative(into);
    }

    private native void dumpNative(String path);

    private native void groupByMemorySize(boolean enable);

    private native void setSamplingNative(double sampling);

    private native void setSampleSizeRangeNative(int minSize, int maxSize);

    private native void enableStacktraceNative(boolean enable);

    private native void enableMmapHookNative(boolean enable);

    private native void xhookRegisterNative(String[] hookSoList);

    private native void xhookIgnoreNative(String[] ignoreSoList);

    private native void xhookInitNative();

    private native void xhookClearNative();

    private native int xhookRefreshNative(boolean async);

    private native void xhookEnableDebugNative(boolean flag);

    private native void xhookEnableSigSegvProtectionNative(boolean flag);
}

