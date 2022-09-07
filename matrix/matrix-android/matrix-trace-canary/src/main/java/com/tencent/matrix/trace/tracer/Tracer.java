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

package com.tencent.matrix.trace.tracer;

import androidx.annotation.CallSuper;

import com.tencent.matrix.lifecycle.owners.ProcessUILifecycleOwner;
import com.tencent.matrix.trace.listeners.LooperObserver;
import com.tencent.matrix.util.MatrixLog;

public abstract class Tracer extends LooperObserver implements ITracer {

    private volatile boolean isAlive = false;
    private static final String TAG = "Matrix.Tracer";

    @CallSuper
    protected void onAlive() {
        MatrixLog.i(TAG, "[onAlive] %s", this.getClass().getName());

    }

    @CallSuper
    protected void onDead() {
        MatrixLog.i(TAG, "[onDead] %s", this.getClass().getName());
    }

    @Override
    final synchronized public void onStartTrace() {
        if (!isAlive) {
            this.isAlive = true;
            onAlive();
        }
    }

    @Override
    final synchronized public void onCloseTrace() {
        if (isAlive) {
            this.isAlive = false;
            onDead();
        }
    }

    @Override
    public void onForeground(boolean isForeground) {

    }

    @Override
    public boolean isAlive() {
        return isAlive;
    }

    public boolean isForeground() {
        return ProcessUILifecycleOwner.INSTANCE.isProcessForeground();
    }
}
