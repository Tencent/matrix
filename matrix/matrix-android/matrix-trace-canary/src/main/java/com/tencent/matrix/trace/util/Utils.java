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

package com.tencent.matrix.trace.util;

import android.os.Looper;

import com.tencent.matrix.util.DeviceUtil;

public class Utils {

    public static String getStack() {
        StackTraceElement[] trace = new Throwable().getStackTrace();
        return getStack(trace);
    }

    public static String getStack(StackTraceElement[] trace) {
        return getStack(trace, "", -1);
    }

    public static String getStack(StackTraceElement[] trace, String preFixStr, int limit) {
        if ((trace == null) || (trace.length < 3)) {
            return "";
        }
        if (limit < 0) {
            limit = Integer.MAX_VALUE;
        }
        StringBuilder t = new StringBuilder(" \n");
        for (int i = 3; i < trace.length - 3 && i < limit; i++) {
            t.append(preFixStr);
            t.append("at ");
            t.append(trace[i].getClassName());
            t.append(":");
            t.append(trace[i].getMethodName());
            t.append("(" + trace[i].getLineNumber() + ")");
            t.append("\n");

        }
        return t.toString();
    }

    public static String getWholeStack(StackTraceElement[] trace, String preFixStr) {
        if ((trace == null) || (trace.length < 3)) {
            return "";
        }

        StringBuilder t = new StringBuilder(" \n");
        for (int i = 0; i < trace.length; i++) {
            t.append(preFixStr);
            t.append("at ");
            t.append(trace[i].getClassName());
            t.append(":");
            t.append(trace[i].getMethodName());
            t.append("(" + trace[i].getLineNumber() + ")");
            t.append("\n");

        }
        return t.toString();
    }

    public static String getWholeStack(StackTraceElement[] trace) {
        StringBuilder stackTrace = new StringBuilder();
        for (StackTraceElement stackTraceElement : trace) {
            stackTrace.append(stackTraceElement.toString()).append("\n");
        }
        return stackTrace.toString();
    }

    public static String getMainThreadJavaStackTrace() {
        StringBuilder stackTrace = new StringBuilder();
        for (StackTraceElement stackTraceElement : Looper.getMainLooper().getThread().getStackTrace()) {
            stackTrace.append(stackTraceElement.toString()).append("\n");
        }
        return stackTrace.toString();
    }

    public static String getJavaStackTrace() {
        StringBuilder stackTrace = new StringBuilder();
        for (StackTraceElement stackTraceElement : Thread.currentThread().getStackTrace()) {
            stackTrace.append(stackTraceElement.toString()).append("\n");
        }
        return stackTrace.toString();
    }

    public static String calculateCpuUsage(long threadMs, long ms) {
        if (threadMs <= 0) {
            return ms > 1000 ? "0%" : "100%";
        }

        if (threadMs >= ms) {
            return "100%";
        }

        return String.format("%.2f", 1.f * threadMs / ms * 100) + "%";
    }

    public static boolean isEmpty(String str) {
        return null == str || str.equals("");
    }

    public static int[] getProcessPriority(int pid) {
        String name = String.format("/proc/%s/stat", pid);
        int priority = Integer.MIN_VALUE;
        int nice = Integer.MAX_VALUE;
        try {
            String content = DeviceUtil.getStringFromFile(name).trim();
            String[] args = content.split(" ");
            if (args.length >= 19) {
                priority = Integer.parseInt(args[17].trim());
                nice = Integer.parseInt(args[18].trim());
            }
        } catch (Exception e) {
            return new int[]{priority, nice};
        }
        return new int[]{priority, nice};
    }


    public static String formatTime(final long timestamp) {
        return new java.text.SimpleDateFormat("[yy-MM-dd HH:mm:ss]").format(new java.util.Date(timestamp));
    }
}
