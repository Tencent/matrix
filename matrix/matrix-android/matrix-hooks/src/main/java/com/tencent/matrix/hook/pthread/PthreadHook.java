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

package com.tencent.matrix.hook.pthread;

import android.text.TextUtils;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.tencent.matrix.hook.AbsHook;
import com.tencent.matrix.hook.HookManager;
import com.tencent.matrix.util.MatrixLog;

import java.util.Arrays;
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
    private ThreadStackShrinkConfig mThreadStackShrinkConfig = null;
    private boolean mHookInstalled             = false;
    private boolean mEnableTracePthreadRelease = false;

    public static class ThreadStackShrinkConfig {
        public boolean enabled = false;
        public final Set<String> ignoreCreatorSoPatterns = new HashSet<>(5);

        public ThreadStackShrinkConfig setEnabled(boolean value) {
            this.enabled = value;
            return this;
        }

        public ThreadStackShrinkConfig setIgnoreCreatorSoPatterns(@Nullable String... patterns) {
            if (patterns == null || patterns.length == 0) {
                ignoreCreatorSoPatterns.clear();
            } else {
                ignoreCreatorSoPatterns.addAll(Arrays.asList(patterns));
            }
            return this;
        }

        public ThreadStackShrinkConfig addIgnoreCreatorSoPatterns(@NonNull String pattern) {
            ignoreCreatorSoPatterns.add(pattern);
            return this;
        }
    }

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

    /**
     * trace pthread_detach or pthread_join
     * @param enabled
     * @return
     */
    public PthreadHook enableTracePthreadRelease(boolean enabled) {
        mEnableTracePthreadRelease = enabled;
        return this;
    }

    public PthreadHook setThreadStackShrinkConfig(@Nullable ThreadStackShrinkConfig config) {
        mThreadStackShrinkConfig = config;
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
        if (TextUtils.isEmpty(path)) {
            throw new IllegalArgumentException("path NOT valid: " + path);
        }
        if (getStatus() == Status.COMMIT_SUCCESS) {
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
    public boolean onConfigure() {
        addHookThreadNameNative(mHookThreadName.toArray(new String[0]));
        enableQuickenNative(mEnableQuicken);
        enableLoggerNative(mEnableLog);
        enableTracePthreadReleaseNative(mEnableTracePthreadRelease);

        if (mThreadStackShrinkConfig != null) {
            final String[] patterns = new String[mThreadStackShrinkConfig.ignoreCreatorSoPatterns.size()];
            if (setThreadStackShrinkIgnoredCreatorSoPatternsNative(
                    mThreadStackShrinkConfig.ignoreCreatorSoPatterns.toArray(patterns))) {
                setThreadStackShrinkEnabledNative(mThreadStackShrinkConfig.enabled);
            } else {
                MatrixLog.e(TAG, "setThreadStackShrinkIgnoredCreatorSoPatternsNative return false, do not enable ThreadStackShrinker.");
                setThreadStackShrinkEnabledNative(false);
            }
        } else {
            setThreadStackShrinkIgnoredCreatorSoPatternsNative(null);
            setThreadStackShrinkEnabledNative(false);
        }
        setThreadTraceEnabledNative(mThreadTraceEnabled);
        mConfigured = true;
        return true;
    }

    @Override
    protected boolean onHook(boolean enableDebug) {
        if (mThreadTraceEnabled || (mThreadStackShrinkConfig != null && mThreadStackShrinkConfig.enabled)) {
            if (!mHookInstalled) {
                installHooksNative(enableDebug);
                mHookInstalled = true;
            }
        }
        return true;
    }

    @Keep
    private native void addHookThreadNameNative(String[] threadNames);

    @Keep
    private native void setThreadTraceEnabledNative(boolean enabled);

    @Keep
    private native void setThreadStackShrinkEnabledNative(boolean enabled);

    @Keep
    private native boolean setThreadStackShrinkIgnoredCreatorSoPatternsNative(String[] patterns);

    @Keep
    private native void enableLoggerNative(boolean enable);

    @Keep
    private native void enableQuickenNative(boolean enable);

    @Keep
    private native void dumpNative(String path);

    @Keep
    private native void installHooksNative(boolean enableDebug);

    @Keep
    private native void enableTracePthreadReleaseNative(boolean enable);
}
