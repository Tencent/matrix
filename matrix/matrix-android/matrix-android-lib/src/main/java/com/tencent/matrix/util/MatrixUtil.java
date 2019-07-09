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

package com.tencent.matrix.util;

import android.app.ActivityManager;
import android.content.Context;
import android.os.Looper;
import android.util.Log;

import java.io.BufferedReader;
import java.io.Closeable;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.List;

/**
 * Created by zhangshaowen on 17/6/1.
 */

public final class MatrixUtil {
    private static final String TAG = "Matrix.MatrixUtil";
    private static String processName = null;

    private MatrixUtil() {
    }

    public static String formatTime(String format, final long timeMilliSecond) {
        return new java.text.SimpleDateFormat(format).format(new java.util.Date(timeMilliSecond));
    }

    public static boolean isInMainThread(final long threadId) {
        return Looper.getMainLooper().getThread().getId() == threadId;
    }

    public static boolean isInMainProcess(Context context) {
        String pkgName = context.getPackageName();
        String processName = getProcessName(context);
        if (processName == null || processName.length() == 0) {
            processName = "";
        }

        return pkgName.equals(processName);
    }

    /**
     * add process name cache
     *
     * @param context
     * @return
     */
    public static String getProcessName(final Context context) {
        if (processName != null) {
            return processName;
        }
        //will not null
        processName = getProcessNameInternal(context);
        return processName;
    }

    private static String getProcessNameInternal(final Context context) {
        int myPid = android.os.Process.myPid();

        if (context == null || myPid <= 0) {
            return "";
        }

        ActivityManager.RunningAppProcessInfo myProcess = null;
        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);

        if (activityManager != null) {
            List<ActivityManager.RunningAppProcessInfo> appProcessList = activityManager
                    .getRunningAppProcesses();

            if (appProcessList != null) {
                try {
                    for (ActivityManager.RunningAppProcessInfo process : appProcessList) {
                        if (process.pid == myPid) {
                            myProcess = process;
                            break;
                        }
                    }
                } catch (Exception e) {
                    Log.e(TAG, "getProcessNameInternal exception:" + e.getMessage());
                }

                if (myProcess != null) {
                    return myProcess.processName;
                }
            }
        }

        byte[] b = new byte[128];
        FileInputStream in = null;
        try {
            in = new FileInputStream("/proc/" + myPid + "/cmdline");
            int len = in.read(b);
            if (len > 0) {
                for (int i = 0; i < len; i++) { // lots of '0' in tail , remove them
                    if (b[i] <= 0) {
                        len = i;
                        break;
                    }
                }
                return new String(b, 0, len, Charset.forName("UTF-8"));
            }

        } catch (Exception e) {
            Log.e(TAG, "getProcessNameInternal exception:" + e.getMessage());
        } finally {
            try {
                if (in != null) {
                    in.close();
                }
            } catch (Exception e) {
                Log.e(TAG, e.getMessage());
            }
        }
        return "";
    }


    public static String getLatestStack(String stack, int count) {
        if (stack == null || stack.isEmpty()) {
            return "";
        }
        String[] strings = stack.split("\n");
        if (strings.length <= count) {
            return stack;
        }
        StringBuffer sb = new StringBuffer(count);
        for (int i = 0; i < count; i++) {
            sb.append(strings[i]).append('\n');
        }
        return sb.toString();
    }

    /**
     * Closes the given {@code Closeable}. Suppresses any IO exceptions.
     */
    public static void closeQuietly(Closeable closeable) {
        try {
            if (closeable != null) {
                closeable.close();
            }
        } catch (IOException e) {
            Log.w(TAG, "Failed to close resource", e);
        }
    }

    private static char[] hexDigits = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    private final static ThreadLocal<MessageDigest> MD5_DIGEST = new ThreadLocal<MessageDigest>() {
        @Override
        protected MessageDigest initialValue() {
            try {
                return MessageDigest.getInstance("MD5");
            } catch (NoSuchAlgorithmException e) {
                throw new RuntimeException("Initialize MD5 failed.", e);
            }
        }
    };

    public static String getMD5String(String s) {
        return getMD5String(s.getBytes());
    }

    public static String getMD5String(byte[] bytes) {
        MessageDigest digest = MD5_DIGEST.get();
        return bufferToHex(digest.digest(bytes));
    }

    private static String bufferToHex(byte[] bytes) {
        return bufferToHex(bytes, 0, bytes.length);
    }

    private static String bufferToHex(byte[] bytes, int m, int n) {
        StringBuffer stringbuffer = new StringBuffer(2 * n);
        int k = m + n;
        for (int l = m; l < k; l++) {
            appendHexPair(bytes[l], stringbuffer);
        }
        return stringbuffer.toString();
    }

    private static void appendHexPair(byte bt, StringBuffer stringbuffer) {
        char c0 = hexDigits[(bt & 0xf0) >> 4];
        char c1 = hexDigits[bt & 0xf];
        stringbuffer.append(c0);
        stringbuffer.append(c1);
    }

    public static String getStringFromFile(String filePath) throws IOException {
        File fl = new File(filePath);
        FileInputStream fin = null;
        String ret;
        try {
            fin = new FileInputStream(fl);
            ret = convertStreamToString(fin);
        } finally {
            if (null != fin) {
                fin.close();
            }
        }
        return ret;
    }

    public static String convertStreamToString(InputStream is) throws IOException {
        BufferedReader reader = null;
        StringBuilder sb = new StringBuilder();
        try {
            reader = new BufferedReader(new InputStreamReader(is, "UTF-8"));
            String line;
            while ((line = reader.readLine()) != null) {
                sb.append(line).append('\n');
            }
        } finally {
            if (null != reader) {
                reader.close();
            }
        }

        return sb.toString();
    }
}
