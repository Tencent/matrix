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

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.os.Build;
import android.os.Looper;
import android.util.ArrayMap;
import android.util.Log;

import java.io.BufferedReader;
import java.io.Closeable;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.reflect.Field;
import java.math.BigInteger;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * Created by zhangshaowen on 17/6/1.
 */

public final class MatrixUtil {
    private static final String TAG = "Matrix.MatrixUtil";
    private static String processName = null;
    private static String packageName = null;

    private MatrixUtil() {
    }

    public static String formatTime(String format, final long timeMilliSecond) {
        return new java.text.SimpleDateFormat(format).format(new java.util.Date(timeMilliSecond));
    }

    @Deprecated
    public static boolean isInMainThread(final long threadId) {
        return Looper.getMainLooper().getThread().getId() == threadId;
    }

    public static boolean isInMainThread() {
        return Looper.myLooper() == Looper.getMainLooper();
    }

    public static boolean isInMainProcess(Context context) {
        String pkgName = context.getPackageName();
        String processName = getProcessName(context);
        if (processName == null || processName.length() == 0) {
            processName = "";
        }

        return pkgName.equals(processName);
    }

    public static boolean isMainProcessName(String processName, Context context) {
        String pkgName = context.getPackageName();
        return Objects.equals(pkgName, processName);
    }

    public static String getPackageName(final Context context) {
        if (packageName != null) {
            return packageName;
        }
        packageName = context.getPackageName();
        return packageName;
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

    public static String printException(Exception e) {
        final StackTraceElement[] stackTrace = e.getStackTrace();
        if ((stackTrace == null)) {
            return "";
        }

        StringBuilder t = new StringBuilder(e.toString());
        for (int i = 2; i < stackTrace.length; i++) {
            t.append('[');
            t.append(stackTrace[i].getClassName());
            t.append(':');
            t.append(stackTrace[i].getMethodName());
            t.append("(" + stackTrace[i].getLineNumber() + ")]");
            t.append("\n");
        }
        return t.toString();
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

    private final static ThreadLocal<MessageDigest> SHA256_DIGEST = new ThreadLocal<MessageDigest>() {
        @Override
        protected MessageDigest initialValue() {
            try {
                return MessageDigest.getInstance("SHA-256");
            } catch (NoSuchAlgorithmException e) {
                throw new RuntimeException("Initialize SHA256-DIGEST failed.", e);
            }
        }
    };

    private static byte[] getSHA(String input) throws NoSuchAlgorithmException {
        // Static getInstance method is called with hashing SHA
        MessageDigest md = SHA256_DIGEST.get();

        // digest() method called
        // to calculate message digest of an input
        // and return array of byte
        return md.digest(input.getBytes(StandardCharsets.UTF_8));
    }

    private static String toHexString(byte[] hash) {
        // Convert byte array into signum representation
        BigInteger number = new BigInteger(1, hash);

        // Convert message digest into hex value
        StringBuilder hexString = new StringBuilder(number.toString(16));

        // Pad with leading zeros
        while (hexString.length() < 32) {
            hexString.insert(0, '0');
        }

        return hexString.toString();
    }

    public static String getSHA256String(String s) throws NoSuchAlgorithmException {
        return toHexString(getSHA(s));
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

    public static long parseLong(final String string, final long def) {
        try {
            return (string == null || string.length() <= 0) ? def : Long.decode(string);
        } catch (NumberFormatException e) {
            MatrixLog.w(TAG, "parseLong error: " + e.getMessage());
        }
        return def;
    }

    public static void printFileByLine(String printTAG, String filePath) throws IOException {
        BufferedReader reader = null;
        try {
            reader = new BufferedReader(new InputStreamReader(new FileInputStream(new File(filePath)), "UTF-8"));
            String line;
            while ((line = reader.readLine()) != null) {
                MatrixLog.i(printTAG, line);
            }
        } catch (Throwable t) {
            MatrixLog.e(printTAG, "printFileByLine failed e : " + t.getMessage());
        } finally {
            if (null != reader) {
                reader.close();
            }
        }
    }

    public static String getTopActivityName() {
        long start = System.currentTimeMillis();
        try {
            Class activityThreadClass = Class.forName("android.app.ActivityThread");
            Object activityThread = activityThreadClass.getMethod("currentActivityThread").invoke(null);
            Field activitiesField = activityThreadClass.getDeclaredField("mActivities");
            activitiesField.setAccessible(true);

            Map<Object, Object> activities;
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
                activities = (HashMap<Object, Object>) activitiesField.get(activityThread);
            } else {
                activities = (ArrayMap<Object, Object>) activitiesField.get(activityThread);
            }
            if (activities.size() < 1) {
                return null;
            }
            for (Object activityRecord : activities.values()) {
                Class activityRecordClass = activityRecord.getClass();
                Field pausedField = activityRecordClass.getDeclaredField("paused");
                pausedField.setAccessible(true);
                if (!pausedField.getBoolean(activityRecord)) {
                    Field activityField = activityRecordClass.getDeclaredField("activity");
                    activityField.setAccessible(true);
                    Activity activity = (Activity) activityField.get(activityRecord);
                    return activity.getClass().getName();
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            long cost = System.currentTimeMillis() - start;
            MatrixLog.d(TAG, "[getTopActivityName] Cost:%s", cost);
        }
        return null;
    }
}
