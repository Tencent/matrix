/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
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

package com.tencent.matrix.trace.hacker;

import android.os.Handler;
import android.os.Message;

import com.tencent.matrix.trace.core.MethodBeat;

/**
 * Created by caichongyang on 2017/5/26.
 *
 **/

public class HackCallback implements Handler.Callback {
    private static final String TAG = "Matrix.HackCallback";
    private static final int LAUNCH_ACTIVITY = 100;
    private static final int ENTER_ANIMATION_COMPLETE = 149;
    private static final int CREATE_SERVICE = 114;
    private static final int RECEIVER = 113;
    private static boolean isCreated = false;

    private final Handler.Callback mOriginalCallback;

    public HackCallback(Handler.Callback callback) {
        this.mOriginalCallback = callback;
    }

    @Override
    public boolean handleMessage(Message msg) {
//        MatrixLog.i(TAG, "[handleMessage] msg.what:%s begin:%s", msg.what, System.currentTimeMillis());
        if (msg.what == LAUNCH_ACTIVITY) {
            Hacker.isEnterAnimationComplete = false;
        } else if (msg.what == ENTER_ANIMATION_COMPLETE) {
            Hacker.isEnterAnimationComplete = true;
        }
        if (!isCreated) {
            if (msg.what == LAUNCH_ACTIVITY || msg.what == CREATE_SERVICE || msg.what == RECEIVER) {
                Hacker.sApplicationCreateEndTime = System.currentTimeMillis();
                Hacker.sApplicationCreateEndMethodIndex = MethodBeat.getCurIndex();
                Hacker.sApplicationCreateScene = msg.what;
                isCreated = true;
            }
        }
        if (null == mOriginalCallback) {
            return false;
        }
        return mOriginalCallback.handleMessage(msg);
    }
}
