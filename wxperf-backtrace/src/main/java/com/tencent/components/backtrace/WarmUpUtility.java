package com.tencent.components.backtrace;

import android.content.Context;
import android.os.CancellationSignal;

import com.tencent.stubs.logger.Log;

import java.io.File;
import java.io.FileFilter;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;

class WarmUpUtility {

    private final static String TAG = "Matrix.Backtrace.WarmUp";

    private final static String DIR_WECHAT_BACKTRACE = "wechat-backtrace";
    private final static String FILE_DEFAULT_SAVING_PATH = "saving-cache";
    private final static String FILE_WARMED_UP = "warmed-up";
    private final static String FILE_CLEAN_UP_TIMESTAMP = "clean-up.timestamp";

    final static long DURATION_LAST_ACCESS_EXPIRED = 15L * 24 * 3600 * 1000; // milliseconds
    final static long DURATION_CLEAN_UP_EXPIRED = 3L * 24 * 3600 * 1000; // milliseconds
    final static long DURATION_CLEAN_UP = 7L * 24 * 3600 * 1000; // milliseconds

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
                Log.e(TAG, "Saving path should under private storage path %s",
                        configuration.mContext.getFilesDir().getParentFile().getAbsolutePath());
            }
        } catch (IOException e) {
            Log.printStack(Log.ERROR, TAG, e);
        }
        return false;
    }

    static boolean needCleanUp(Context context) {
        File timestamp = cleanUpTimestampFile(context);
        if (!timestamp.exists()) {
            try {
                // Create clean-up timestamp file if necessary.
                timestamp.createNewFile();
            } catch (IOException e) {
                Log.printStack(Log.ERROR, TAG, e);
            }
            return false;
        }
        return System.currentTimeMillis() - timestamp.lastModified() >= DURATION_CLEAN_UP;
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
            Log.printStack(Log.ERROR, TAG, e);
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

    static String readFileContent(File file) {
        if (file.isFile()) {
            FileReader reader = null;
            try {
                StringBuilder sb = new StringBuilder(4096);
                reader = new FileReader(file);
                char[] buffer = new char[1024];
                int len;
                while ((len = reader.read(buffer)) > 0) {
                    sb.append(buffer, 0, len);
                }

                return sb.toString();
            } catch (Exception e) {
                Log.printStack(Log.ERROR, TAG, e);
            } finally {
                if (reader != null) {
                    try {
                        reader.close();
                    } catch (IOException e) {
                        Log.printStack(Log.ERROR, TAG, e);
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
                Log.printStack(Log.ERROR, TAG, e);
            } finally {
                if (writer != null) {
                    try {
                        writer.close();
                    } catch (IOException e) {
                        Log.printStack(Log.ERROR, TAG, e);
                    }
                }
            }
        }

        return false;
    }

}
