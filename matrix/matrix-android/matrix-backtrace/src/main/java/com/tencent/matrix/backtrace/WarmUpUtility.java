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

package com.tencent.matrix.backtrace;

import android.content.Context;
import android.os.CancellationSignal;

import com.tencent.matrix.util.MatrixLog;

import java.io.File;
import java.io.FileFilter;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

public class WarmUpUtility {

    private final static String TAG = "Matrix.Backtrace.WarmUp";

    private final static String DIR_WECHAT_BACKTRACE = "wechat-backtrace";
    private final static String FILE_DEFAULT_SAVING_PATH = "saving-cache";
    private final static String FILE_WARMED_UP = "warmed-up";
    private final static String FILE_DISK_USAGE = "disk-usage.timestamp";
    private final static String FILE_CLEAN_UP_TIMESTAMP = "clean-up.timestamp";
    private final static String FILE_BLOCKED_LIST = "blocked-list";
    private final static String FILE_UNFINISHED = "unfinished";

    final static long DURATION_LAST_ACCESS_FAR_FUTURE = 30L * 24 * 3600 * 1000; // milliseconds
    final static long DURATION_LAST_ACCESS_EXPIRED = 60L * 24 * 3600 * 1000; // milliseconds
    final static long DURATION_CLEAN_UP_EXPIRED = 3L * 24 * 3600 * 1000; // milliseconds
    final static long DURATION_CLEAN_UP = 3L * 24 * 3600 * 1000; // milliseconds
    final static long DURATION_DISK_USAGE_COMPUTATION = 3L * 24 * 3600 * 1000; // milliseconds

    final static int WARM_UP_FILE_MAX_RETRY = 3;

    final static String UNFINISHED_KEY_SPLIT = ":";
    final static String UNFINISHED_RETRY_SPLIT = "|";

    public static class UnfinishedManagement {

        private static Map<String, Integer> mUnfinishedWarmUp;

        private static int retryCount(Context context, String key) {
            if (mUnfinishedWarmUp == null) {
                mUnfinishedWarmUp = WarmUpUtility.readUnfinishedMaps(context);
            }
            Integer value = mUnfinishedWarmUp.get(key);
            int retryCount = value != null ? value : 0;
            return retryCount;
        }

        public static boolean check(Context context, String pathOfElf, int offset) {
            String key = WarmUpUtility.unfinishedKey(pathOfElf, offset);
            int retryCount = retryCount(context, key);
            if (retryCount >= WARM_UP_FILE_MAX_RETRY) {
                return false;
            }

            return true;
        }

        public static boolean checkAndMark(Context context, String pathOfElf, int offset) {
            String key = WarmUpUtility.unfinishedKey(pathOfElf, offset);
            int retryCount = retryCount(context, key);
            if (retryCount >= WARM_UP_FILE_MAX_RETRY) {
                return false;
            } else {
                mUnfinishedWarmUp.put(key, retryCount + 1);
            }

            flushUnfinishedMaps(context, mUnfinishedWarmUp);

            return true;
        }

        public static void result(Context context, String pathOfElf, int offset, boolean success) {
            String key = WarmUpUtility.unfinishedKey(pathOfElf, offset);
            int retryCount = retryCount(context, key);
            if (success) {
                mUnfinishedWarmUp.remove(key);
            } else {
                mUnfinishedWarmUp.put(key, retryCount + 1);
            }
//
//            mUnfinishedWarmUp.put(key, retryCount + 1);

            flushUnfinishedMaps(context, mUnfinishedWarmUp);
        }
    }

    static File cleanUpTimestampFile(Context context) {
        File file = new File(context.getFilesDir().getAbsolutePath() + "/"
                + DIR_WECHAT_BACKTRACE + "/" + FILE_CLEAN_UP_TIMESTAMP);
        file.getParentFile().mkdirs();
        return file;
    }

    static File warmUpMarkedFile(Context context) {
        File file = new File(context.getFilesDir().getAbsolutePath() + "/"
                + DIR_WECHAT_BACKTRACE + "/" + FILE_WARMED_UP);
        file.getParentFile().mkdirs();
        return file;
    }

    static File diskUsageFile(Context context) {
        File file = new File(context.getFilesDir().getAbsolutePath() + "/"
                + DIR_WECHAT_BACKTRACE + "/" + FILE_DISK_USAGE);
        file.getParentFile().mkdirs();
        return file;
    }

    static String defaultSavingPath(WeChatBacktrace.Configuration configuration) {
        return configuration.mContext.getFilesDir().getAbsolutePath() + "/"
                + DIR_WECHAT_BACKTRACE + "/" + FILE_DEFAULT_SAVING_PATH + "/";
    }

    static String validateSavingPath(WeChatBacktrace.Configuration configuration) {
        if (pathValidation(configuration)) {
            return configuration.mSavingPath;
        } else {
            return defaultSavingPath(configuration);
        }
    }

    static boolean pathValidation(WeChatBacktrace.Configuration configuration) {
        if (configuration.mSavingPath == null) {
            return false;
        }

        File savingPath = new File(configuration.mSavingPath);
        try {
            if (savingPath.getCanonicalPath().startsWith(
                    configuration.mContext.getFilesDir().getParentFile()
                            .getCanonicalFile().getAbsolutePath())) {
                return true;
            } else {
                MatrixLog.e(TAG, "Saving path should under private storage path %s",
                        configuration.mContext.getFilesDir().getParentFile().getAbsolutePath());
            }
        } catch (IOException e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }
        return false;
    }

    static File unfinishedFile(Context context) {
        File file = new File(context.getFilesDir().getAbsolutePath() + "/"
                + DIR_WECHAT_BACKTRACE + "/" + FILE_UNFINISHED);
        file.getParentFile().mkdirs();
        if (!file.exists()) {
            try {
                file.createNewFile();
            } catch (IOException e) {
                MatrixLog.printErrStackTrace(TAG, e, "");
            }
        }
        return file;
    }

    static String unfinishedKey(String pathOfElf, int offset) {
        return pathOfElf + UNFINISHED_KEY_SPLIT + offset;
    }

    static void flushUnfinishedMaps(Context context, Map<String, Integer> unfinished) {
        File file = unfinishedFile(context);

        StringBuffer sb = new StringBuffer();
        for (Map.Entry<String, Integer> entry : unfinished.entrySet()) {
            sb.append(entry.getKey() + UNFINISHED_RETRY_SPLIT + entry.getValue() + "\n");
        }

        writeContentToFile(file, sb.toString());
    }

    static Map<String, Integer> readUnfinishedMaps(Context context) {
        HashMap<String, Integer> maps = new HashMap<>();
        File file = unfinishedFile(context);
        String content = readFileContent(file, 512_000);
        if (content == null) {

            MatrixLog.w(TAG, "Read unfinished maps file failed, file size %s", file.length());

            if (file.length() > 512_000) {
                file.delete();
            }
        } else {
            String[] lines = content.split("\n");
            for (String line : lines) {
                int index = line.lastIndexOf(UNFINISHED_RETRY_SPLIT);
                if (index < 0) {
                    continue;
                }
                try {
                    String key = line.substring(0, index);
                    String value = line.substring(index + 1);
                    maps.put(key, Integer.parseInt(value));
                } catch (Throwable ignore) {
                    MatrixLog.printErrStackTrace(TAG, ignore, "");
                }
            }
        }

        return maps;
    }

    static boolean needCleanUp(Context context) {
        File timestamp = cleanUpTimestampFile(context);
        if (!timestamp.exists()) {
            try {
                // Create clean-up timestamp file if necessary.
                timestamp.createNewFile();
            } catch (IOException e) {
                MatrixLog.printErrStackTrace(TAG, e, "");
            }
            return false;
        }
        return System.currentTimeMillis() - timestamp.lastModified() >= DURATION_CLEAN_UP;
    }

    static boolean shouldComputeDiskUsage(Context context) {
        File timestamp = diskUsageFile(context);
        if (!timestamp.exists()) {
            try {
                // Create disk usage timestamp file if necessary.
                timestamp.createNewFile();
            } catch (IOException e) {
                MatrixLog.printErrStackTrace(TAG, e, "");
            }
            return false;
        }
        return System.currentTimeMillis() - timestamp.lastModified() >= DURATION_DISK_USAGE_COMPUTATION;
    }

    static void markComputeDiskUsageTimestamp(Context context) {
        File timestamp = WarmUpUtility.diskUsageFile(context);
        try {
            timestamp.createNewFile();
            timestamp.setLastModified(System.currentTimeMillis());
        } catch (IOException e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }
    }

    static boolean hasWarmedUp(Context context) {
        return WarmUpUtility.warmUpMarkedFile(context).exists();
    }

    static void markCleanUpTimestamp(Context context) {
        File timestamp = WarmUpUtility.cleanUpTimestampFile(context);
        try {
            timestamp.createNewFile();
            timestamp.setLastModified(System.currentTimeMillis());
        } catch (IOException e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }
    }

    static void iterateTargetDirectory(File target, CancellationSignal cs, FileFilter filter) {
        if (target.isDirectory()) {
            File[] files = target.listFiles();
            if (files != null) {
                for (File file : files) {
                    iterateTargetDirectory(file, cs, filter);
                    cs.throwIfCanceled();
                }
            }
        } else {
            filter.accept(target);
            cs.throwIfCanceled();
        }
    }

    static String readFileContent(File file, int max) {
        if (file.isFile()) {
            FileReader reader = null;
            try {
                StringBuilder sb = new StringBuilder(4096);
                reader = new FileReader(file);
                char[] buffer = new char[1024];
                int accumulated = 0;
                int len;
                while ((len = reader.read(buffer)) > 0) {
                    sb.append(buffer, 0, len);
                    accumulated += len;
                    if (accumulated > max) {
                        return null;
                    }
                }

                return sb.toString();
            } catch (Exception e) {
                MatrixLog.printErrStackTrace(TAG, e, "");
            } finally {
                if (reader != null) {
                    try {
                        reader.close();
                    } catch (IOException e) {
                        MatrixLog.printErrStackTrace(TAG, e, "");
                    }
                }
            }
        }
        return null;
    }

    static boolean writeContentToFile(File file, String content) {
        if (file.isFile()) {
            FileWriter writer = null;
            try {
                writer = new FileWriter(file);
                writer.write(content);
                return true;
            } catch (Exception e) {
                MatrixLog.printErrStackTrace(TAG, e, "");
            } finally {
                if (writer != null) {
                    try {
                        writer.close();
                    } catch (IOException e) {
                        MatrixLog.printErrStackTrace(TAG, e, "");
                    }
                }
            }
        }

        return false;
    }

}
