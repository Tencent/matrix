package com.tencent.matrix.hook.pthread;

import android.text.TextUtils;

import androidx.annotation.Keep;
import androidx.annotation.Nullable;

import com.tencent.matrix.hook.AbsHook;
import com.tencent.matrix.hook.HookManager;
import com.tencent.matrix.util.MatrixLog;

import java.util.HashSet;
import java.util.Set;

/**
 * Created by Yves on 2020-03-11
 */
public class PthreadHook extends AbsHook {
    private static final String TAG = "Matrix.Pthread";

    public static final PthreadHook INSTANCE = new PthreadHook();

    private Set<String> mHookThreadName = new HashSet<>();

    private boolean mEnableQuicken = false;

    private boolean mEnableLog = false;

    private boolean mConfigured = false;
    private boolean mThreadTraceEnabled = false;
    private boolean mThreadStackShinkEnabled = false;

    private PthreadHook() {
    }

    public PthreadHook addHookThread(String regex) {
        if (TextUtils.isEmpty(regex)) {
            MatrixLog.e(TAG, "thread regex is empty!!!");
        } else {
            mHookThreadName.add(regex);
        }
        return this;
    }

    public PthreadHook addHookThread(String... regexArr) {
        for (String regex : regexArr) {
            addHookThread(regex);
        }
        return this;
    }

    public PthreadHook setThreadTraceEnabled(boolean enabled) {
        mThreadTraceEnabled = enabled;
        return this;
    }

    public PthreadHook setThreadStackShinkEnabled(boolean enabled) {
        mThreadStackShinkEnabled = enabled;
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

    public void dump(String path) {
        if (HookManager.INSTANCE.hasHooked()) {
            dumpNative(path);
        }
    }

    public void enableQuicken(boolean enable) {
        mEnableQuicken = enable;
        if (mConfigured) {
            enableQuickenNative(mEnableQuicken);
        }
    }

    public void enableLogger(boolean enable) {
        mEnableLog = enable;
        if (mConfigured) {
            enableLoggerNative(mEnableLog);
        }
    }

    @Nullable
    @Override
    protected String getNativeLibraryName() {
        return "matrix-pthreadhook";
    }

    @Override
    public void onConfigure() {
        addHookThreadNameNative(mHookThreadName.toArray(new String[0]));
        enableQuickenNative(mEnableQuicken);
        enableLoggerNative(mEnableLog);
        setThreadStackShinkEnabledNative(mThreadStackShinkEnabled);
        setThreadTraceEnabledNative(mThreadTraceEnabled);
        mConfigured = true;
    }

    @Override
    protected void onHook(boolean enableDebug) {
        if (mThreadTraceEnabled || mThreadStackShinkEnabled) {
            installHooksNative(enableDebug);
        }
    }

    @Keep
    private native void addHookThreadNameNative(String[] threadNames);

    @Keep
    private native void setThreadTraceEnabledNative(boolean enabled);

    @Keep
    private native void setThreadStackShinkEnabledNative(boolean enabled);

    @Keep
    private native void enableLoggerNative(boolean enable);

    @Keep
    private native void enableQuickenNative(boolean enable);

    @Keep
    private native void dumpNative(String path);

    @Keep
    private native void installHooksNative(boolean enableDebug);
}
