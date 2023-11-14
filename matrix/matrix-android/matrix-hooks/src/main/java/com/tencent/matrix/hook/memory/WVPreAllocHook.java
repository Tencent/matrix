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

import android.os.Build;

import androidx.annotation.NonNull;

import com.tencent.matrix.hook.AbsHook;

public class WVPreAllocHook extends AbsHook {
    public static final WVPreAllocHook INSTANCE = new WVPreAllocHook();

    @NonNull
    @Override
    protected String getNativeLibraryName() {
        return "matrix-memoryhook";
    }

    @Override
    protected boolean onConfigure() {
        // Ignored.
        return true;
    }

    @Override
    protected boolean onHook(boolean enableDebug) {
        return installHooksNative(Build.VERSION.SDK_INT, this.getClass().getClassLoader(), enableDebug);
    }

    private native boolean installHooksNative(int sdkVer, ClassLoader classLoader, boolean enableDebug);
}
