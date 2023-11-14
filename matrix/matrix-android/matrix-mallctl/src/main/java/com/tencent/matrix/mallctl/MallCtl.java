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

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

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

    public synchronized static boolean jeSetRetain(boolean enable) {
        try {
            return setRetainNative(enable);
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "set retain failed");
        }
        return false;
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

    public interface TrimPrediction {
        boolean canBeTrim(String pathName, String permission);
    }

    public static class DefaultPrediction implements TrimPrediction {
        @Override
        public boolean canBeTrim(String pathName, String permission) {
            if (pathName.endsWith(" (deleted)")) {
                pathName = pathName.substring(0, pathName.length() - " (deleted)".length());
            } else if (pathName.endsWith("]")) {
                pathName = pathName.substring(0, pathName.length() - "]".length());
            }
            return !permission.contains("w")
                    && (pathName.endsWith(".so")
                    || pathName.endsWith(".dex")
                    || pathName.endsWith(".apk")
                    || pathName.endsWith(".vdex")
                    || pathName.endsWith(".odex")
                    || pathName.endsWith(".oat")
                    || pathName.endsWith(".art")
                    || pathName.endsWith(".ttf")
                    || pathName.endsWith(".otf")
                    || pathName.endsWith(".jar"));
        }
    }

    public synchronized static void flushReadOnlyFilePages(TrimPrediction prediction) {
        if (prediction == null) {
            prediction = new DefaultPrediction();
        }
        Pattern pattern = Pattern.compile("^([0-9a-f]+)-([0-9a-f]+)\\s+([rwxps-]{4})\\s+[0-9a-f]+\\s+[0-9a-f]+:[0-9a-f]+\\s+\\d+\\s*(.*)$");
        try (BufferedReader br = new BufferedReader(new InputStreamReader(new FileInputStream("/proc/self/maps")))) {
            String line = br.readLine();
            while (line != null) {
                Matcher matcher = pattern.matcher(line);
                if (matcher.find()) {
                    String beginStr = matcher.group(1);
                    String endStr = matcher.group(2);
                    String permission = matcher.group(3);
                    String name = matcher.group(4);
                    if (name == null || name.isEmpty()) {
                        name = "[no-name]";
                    }
                    if (prediction.canBeTrim(name, permission) && beginStr != null && endStr != null) {
                        try {
                            long beginPtr = Long.parseLong(beginStr, 16);
                            long endPtr = Long.parseLong(endStr, 16);
                            long size = endPtr - beginPtr;
                            flushReadOnlyFilePagesNative(beginPtr, size);
                        } catch (Throwable e) {
                            MatrixLog.printErrStackTrace(TAG, e, "%s-%s %s %s", beginStr, endStr, permission, name);
                        }
                    }
                }
                line = br.readLine();
            }
        } catch (IOException e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }
    }

    @Keep
    private static native void initNative();

    @Keep
    private static native String getVersionNative();

    @Keep
    private static native int malloptNative();

    @Keep
    private static native boolean setRetainNative(boolean enable);

    private static native int flushReadOnlyFilePagesNative(long begin, long size);
}
