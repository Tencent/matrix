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

import com.tencent.sqlitelint.SQLiteLint;

import java.io.File;
import java.util.ArrayList;
import java.util.ListIterator;

/**
 * Created by liyongjie on 16/9/26.
 */

public class SQLiteLintUtil {
    private static final String TAG = "SQLiteLint.SQLiteLintUtil";

    public static boolean isNullOrNil(final String object) {
        return object == null || object.length() <= 0;
    }

    public static String nullAsNil(final String object) {
        return object == null ? "" : object;
    }

    public static String extractDbName(String dbPath) {
        if (SQLiteLintUtil.isNullOrNil(dbPath)) {
            return null;
        }

        String dbName = null;
        String[] arr = dbPath.split("/");
        if (arr != null && arr.length > 0) {
            dbName = arr[arr.length - 1];
        }

        return dbName;
    }

    public static int getInt(final String string, final int def) {
        try {
            return (string == null || string.length() <= 0) ? def : Integer.parseInt(string);

        } catch (NumberFormatException e) {
            e.printStackTrace();
        }
        return def;
    }

    public static final String YYYY_MM_DD_HH_mm = "yyyy-MM-dd HH:mm";

    public static String formatTime(String format, final long timeMilliSecond) {
        return new java.text.SimpleDateFormat(format).format(new java.util.Date(timeMilliSecond));
    }

    public static void mkdirs(String filePath) {
        File file = new File(filePath);
        if (!file.exists()) {
            File parentFile = file.getParentFile();
            if (parentFile != null) {
                parentFile.mkdirs();
            }
        }
    }

    private static final int DEFAULT_MAX_STACK_LAYER = 6;
    public static String stackTraceToString(final StackTraceElement[] arr) {
        if (arr == null) {
            return "";
        }

        ArrayList<StackTraceElement> stacks = new ArrayList<>(arr.length);
        for (int i = 0; i < arr.length; i++) {
            String className = arr[i].getClassName();
            // remove unused stacks
            if (className.contains("com.tencent.sqlitelint")) {
                continue;
            }

            stacks.add(arr[i]);
        }
        // stack still too large
        if (stacks.size() > DEFAULT_MAX_STACK_LAYER && SQLiteLint.sPackageName != null) {
            ListIterator<StackTraceElement> iterator = stacks.listIterator(stacks.size());
            // from backward to forward
            while (iterator.hasPrevious()) {
                StackTraceElement stack = iterator.previous();
                String className = stack.getClassName();
                if (!className.contains(SQLiteLint.sPackageName)) {
                    iterator.remove();
                }
                if (stacks.size() <= DEFAULT_MAX_STACK_LAYER) {
                    break;
                }
            }
        }
        StringBuffer sb = new StringBuffer(stacks.size());
        for (StackTraceElement stackTraceElement : stacks) {
            sb.append(stackTraceElement).append('\n');
        }
        return sb.toString();
    }

    public static String getThrowableStack(Throwable throwable) {
        if (throwable == null) {
            return "";
        }
        return stackTraceToString(throwable.getStackTrace());
    }

    public static String getThrowableStack() {
        try {
            return getThrowableStack(new Throwable());
        } catch (Throwable e) {
            SLog.e(TAG, "getThrowableStack ex %s", e.getMessage());
        }
        return "";
    }
}
