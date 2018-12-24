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

package com.tencent.sqlitelint.util;


/**
 * Created by liyongjie on 16/9/26.
 */

import android.util.Log;

import com.tencent.matrix.util.MatrixLog;

public class SLog {
    private static final String TAG = "SQLiteLint.SLog";

    private volatile static SLog mInstance = null;

    public static SLog getInstance() {
        if (mInstance == null) {
            synchronized (SLog.class) {
                if (mInstance == null) {
                    mInstance = new SLog();
                }
            }
        }
        return mInstance;
    }

    public static native void nativeSetLogger(int logLevel);

    public void printLog(int priority, String tag, String msg) {
        try {
            switch (priority) {
                case Log.VERBOSE:
                    MatrixLog.v(tag, msg);
                    return;
                case Log.DEBUG:
                    MatrixLog.d(tag, msg);
                    return;
                case Log.INFO:
                    MatrixLog.i(tag, msg);
                    return;
                case Log.WARN:
                    MatrixLog.w(tag, msg);
                    return;
                case Log.ERROR:
                case Log.ASSERT:
                    MatrixLog.e(tag, msg);
                    return;
                default:
                    MatrixLog.i(tag, msg);
                    return;
            }
        } catch (Throwable e) {
            Log.e(TAG, "printLog ex " + e.getMessage());
        }
    }

    public static void e(final String tag, final String format, final Object... args) {
        getInstance().printLog(Log.ERROR, tag, String.format(format, args));
    }

    public static void w(final String tag, final String format, final Object... args) {
        getInstance().printLog(Log.WARN, tag, String.format(format, args));
    }

    public static void i(final String tag, final String format, final Object... args) {
        getInstance().printLog(Log.INFO, tag, String.format(format, args));
    }

    public static void d(final String tag, final String format, final Object... args) {
        getInstance().printLog(Log.DEBUG, tag, String.format(format, args));
    }

    public static void v(final String tag, final String format, final Object... args) {
        getInstance().printLog(Log.VERBOSE, tag, String.format(format, args));
    }

}

