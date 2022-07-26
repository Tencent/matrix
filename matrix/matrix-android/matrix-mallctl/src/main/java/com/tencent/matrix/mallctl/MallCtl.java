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

package com.tencent.matrix.mallctl;


import androidx.annotation.Keep;

import com.tencent.matrix.util.MatrixLog;

/**
 * Created by Yves on 2020/7/15
 */
public class MallCtl {
    private static final String TAG = "Matrix.JeCtl";

    private static boolean initialized = false;

    static {
        try {
            System.loadLibrary("matrix-mallctl");
            initNative();
            initialized = true;
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }
    }

    public synchronized static String jeVersion() {
        if (!initialized) {
            MatrixLog.e(TAG, "JeCtl init failed! check if so exists");
            return "VER_UNKNOWN";
        }

        return getVersionNative();
    }

    public static final int MALLOPT_FAILED = 0;
    public static final int MALLOPT_SUCCESS = 1;
    public static final int MALLOPT_SYM_NOT_FOUND = -1;
    public static final int MALLOPT_EXCEPTION = -2;

    /**
     * @return On success, mallopt() returns 1.  On error, it returns 0.
     */
    public synchronized static int mallopt() {
        try {
            return malloptNative();
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "mallopt failed");
        }
        return MALLOPT_EXCEPTION;
    }

    @Keep
    private static native void initNative();

    @Keep
    private static native String getVersionNative();

    @Keep
    private static native int malloptNative();
}
