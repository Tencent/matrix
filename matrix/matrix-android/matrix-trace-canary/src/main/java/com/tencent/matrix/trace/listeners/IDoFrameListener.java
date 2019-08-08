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

package com.tencent.matrix.trace.listeners;

import android.os.Handler;

import com.tencent.matrix.util.MatrixHandlerThread;

/**
 * Created by caichongyang on 2017/5/26.
 **/
public class IDoFrameListener {

    private Handler handler;
    public long time;

    public IDoFrameListener() {

    }

    public IDoFrameListener(Handler handler) {
        this.handler = handler;
    }


    public void doFrameAsync(String visibleScene, long frameCost, int droppedFrames) {

    }

    public void doFrameSync(String visibleScene, long frameCost, int droppedFrames) {

    }

    public Handler getHandler() {
        if (handler == null) {
            handler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper());
        }
        return handler;
    }


}
