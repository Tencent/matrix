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


import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.hook.AbsHook;
import com.tencent.matrix.hook.HookManager;

import java.util.HashSet;
import java.util.Set;

/**
 * Created by Yves on 2020-03-11
 */
public class PthreadHook extends AbsHook {
    private static final String TAG = "Matrix.Pthread";

    public static final PthreadHook INSTANCE = new PthreadHook();

    private Set<String> mHookSoSet      = new HashSet<>();
    private Set<String> mIgnoreSoSet    = new HashSet<>();
    private Set<String> mHookThreadName = new HashSet<>();

    private boolean mEnableQuicken = false;

    private boolean mConfigured = false;

    private PthreadHook() {
    }

    public PthreadHook addHookSo(String regex) {
        if (TextUtils.isEmpty(regex)) {
            MatrixLog.e(TAG, "so regex is empty");
        } else {
            mHookSoSet.add(regex);
        }
        return this;
    }

    public PthreadHook addHookSo(String... regexArr) {
        for (String regex : regexArr) {
            addHookSo(regex);
        }
        return this;
    }

    public PthreadHook addIgnoreSo(String regex) {
        if (TextUtils.isEmpty(regex)) {
            return this;
        }
        mIgnoreSoSet.add(regex);
        return this;
    }

    public PthreadHook addIgnoreSo(String... regexArr) {
        for (String regex : regexArr) {
            addIgnoreSo(regex);
        }
        return this;
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

    @Override
    public void onConfigure() {
        addHookThreadNameNative(mHookThreadName.toArray(new String[0]));
        enableQuickenNative(mEnableQuicken);
        mConfigured = true;
    }

    @Override
    protected void onHook() {
        addHookSoNative(mHookSoSet.toArray(new String[0]));
        addIgnoreSoNative(mIgnoreSoSet.toArray(new String[0]));
    }

    private native void addHookSoNative(String[] hookSoList);

    private native void addIgnoreSoNative(String[] hookSoList);

    private native void addHookThreadNameNative(String[] threadNames);

    private native void dumpNative(String path);

    private native void enableQuickenNative(boolean enable);

}
