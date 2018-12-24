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

import com.tencent.matrix.trace.core.MethodBeat;
import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Field;
/**
 * Created by caichongyang on 2017/5/26.
 *
 **/
public class Hacker {
    private static final String TAG = "Matrix.Hacker";
    public static boolean isEnterAnimationComplete = false;
    public static long sApplicationCreateBeginTime = 0L;
    public static int sApplicationCreateBeginMethodIndex = 0;
    public static long sApplicationCreateEndTime = 0L;
    public static int sApplicationCreateEndMethodIndex = 0;
    public static int sApplicationCreateScene = -100;

    public static void hackSysHandlerCallback() {
        try {
            sApplicationCreateBeginTime = System.currentTimeMillis();
            sApplicationCreateBeginMethodIndex = MethodBeat.getCurIndex();
            Class<?> forName = Class.forName("android.app.ActivityThread");
            Field field = forName.getDeclaredField("sCurrentActivityThread");
            field.setAccessible(true);
            Object activityThreadValue = field.get(forName);
            Field mH = forName.getDeclaredField("mH");
            mH.setAccessible(true);
            Object handler = mH.get(activityThreadValue);
            Class<?> handlerClass = handler.getClass().getSuperclass();
            Field callbackField = handlerClass.getDeclaredField("mCallback");
            callbackField.setAccessible(true);
            Handler.Callback originalCallback = (Handler.Callback) callbackField.get(handler);
            HackCallback callback = new HackCallback(originalCallback);
            callbackField.set(handler, callback);
            MatrixLog.i(TAG, "hook system handler completed. start:%s", sApplicationCreateBeginTime);
        } catch (Exception e) {
            MatrixLog.e(TAG, "hook system handler err! %s", e.getCause().toString());
        }
    }
}
