package com.tencent.matrix.memguard;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.os.Process;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.tencent.matrix.hook.AbsHook;
import com.tencent.matrix.hook.memory.MemoryHook;
import com.tencent.matrix.util.MatrixLog;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FilenameFilter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

public final class MemGuard {
    private static final String TAG = "MemGuard";

    private static final String HOOK_COMMON_NATIVE_LIB_NAME = "matrix-hookcommon";
    private static final String NATIVE_LIB_NAME = "matrix-memguard";
    private static final String ISSUE_CALLBACK_THREAD_NAME = "MemGuard.IssueCB";
    private static final long ISSUE_CALLBACK_TIMEOUT_MS = 5000;
    private static final String DEFAULT_DUMP_FILE_EXT = ".txt";

    private static final boolean[] sInstalled = {false};

    public interface NativeLibLoader {
        void loadLibrary(@NonNull String libraryName);
    }

    public interface IssueCallback {
        void onIssueDumpped(@NonNull String dumpFile) throws Throwable;
    }

    private static IssueCallback sIssueCallback = new IssueCallback() {
        @Override
        public void onIssueDumpped(@NonNull String dumpFile) throws Throwable {
            final File fDumpFile = new File(dumpFile);
            if (!fDumpFile.exists()) {
                MatrixLog.e(TAG, "Dump file %s does not exist, dump failure ?", dumpFile);
                return;
            }
            BufferedReader br = null;
            try {
                br = new BufferedReader(new FileReader(fDumpFile));
                String line = null;
                while ((line = br.readLine()) != null) {
                    MatrixLog.w(TAG, "[DumpedIssue] >> %s", line);
                }
            } finally {
                if (br != null) {
                    try {
                        br.close();
                    } catch (Throwable ignored) {
                        // Ignored.
                    }
                }
            }
        }
    };

    public static boolean install(@NonNull Options opts, @Nullable IssueCallback issueCallback) {
        return install(opts, null, issueCallback);
    }

    public static boolean install(@NonNull Options opts,
                                  @Nullable NativeLibLoader soLoader,
                                  @Nullable IssueCallback issueCallback) {
        Objects.requireNonNull(opts);

        synchronized (sInstalled) {
            if (sInstalled[0]) {
                MatrixLog.w(TAG, "Already installed.");
                return true;
            }

            if (MemoryHook.INSTANCE.getStatus() == AbsHook.Status.COMMIT_SUCCESS) {
                MatrixLog.w(TAG, "MemoryHook has been committed, skip MemGuard install logic.");
                return false;
            }

            boolean success = false;
            try {
                if (soLoader != null) {
                    soLoader.loadLibrary(HOOK_COMMON_NATIVE_LIB_NAME);
                    soLoader.loadLibrary(NATIVE_LIB_NAME);
                } else {
                    System.loadLibrary(HOOK_COMMON_NATIVE_LIB_NAME);
                    System.loadLibrary(NATIVE_LIB_NAME);
                }

                if (issueCallback != null) {
                    sIssueCallback = issueCallback;
                }

                success = nativeInstall(opts);
            } catch (Throwable thr) {
                MatrixLog.printErrStackTrace(TAG, thr, "Install MemGuard failed.");
                success = false;
            }
            if (success) {
                MatrixLog.i(TAG, "Install MemGuard successfully with " + opts);
                MemoryHook.INSTANCE.notifyMemGuardInstalled();
            } else {
                MatrixLog.e(TAG, "Install MemGuard failed with " + opts);
            }
            sInstalled[0] = success;
            return success;
        }
    }

    public static boolean isInstalled() {
        synchronized (sInstalled) {
            return sInstalled[0];
        }
    }

    @NonNull
    public static List<File> getLastIssueDumpFilesInDefaultDir(@NonNull Context context) {
        final File[] subFiles = new File(getDefaultIssueDumpDir(context)).listFiles(new FilenameFilter() {
            @Override
            public boolean accept(File dir, String name) {
                return name.endsWith(DEFAULT_DUMP_FILE_EXT);
            }
        });
        if (subFiles != null) {
            return Collections.unmodifiableList(Arrays.asList(subFiles));
        } else {
            return Collections.emptyList();
        }
    }

    private static native boolean nativeInstall(@NonNull Options opts);

    @Nullable
    private static native String nativeGetIssueDumpFilePath();

    @Keep
    private static void c2jNotifyOnIssueDumped(final String dumpFile) {
        final Thread cbThread = new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    sIssueCallback.onIssueDumpped(dumpFile);
                } catch (Throwable thr) {
                    MatrixLog.printErrStackTrace(TAG, thr, "Exception was thrown when onIssueDumpped was called.");
                }
            }
        }, ISSUE_CALLBACK_THREAD_NAME);
        final long st = System.currentTimeMillis();
        cbThread.start();
        try {
            cbThread.join(ISSUE_CALLBACK_TIMEOUT_MS);
        } catch (InterruptedException e) {
            MatrixLog.w(TAG, "Issue callback was interrupted.");
        }
        final long cost = System.currentTimeMillis() - st;
        if (cost > ISSUE_CALLBACK_TIMEOUT_MS) {
            MatrixLog.w(TAG, "Timeout when call issue callback.");
        }
    }

    public static final class Options {
        public static final int DEFAULT_MAX_ALLOCATION_SIZE = 8 * 1024; // 8K
        public static final int DEFAULT_MAX_DETECTABLE_ALLOCATION_COUNT = 4096;
        public static final int DEFAULT_MAX_SKIPPED_ALLOCATION_COUNT = 5;
        public static final int DEFAULT_PERCENTAGE_OF_LEFT_SIDE_GUARD = 30;
        public static final boolean DEFAULT_PERFECT_RIGHT_SIDE_GUARD = false;
        public static final boolean DEFAULT_IGNORE_OVERLAPPED_READING = false;
        public static final String DEFAULT_TARGET_SO_PATTERN = ".*/lib.*\\.so$";

        /**
         * Max allocation size MemGuard can detect its under/overflow issues.
         */
        @Keep
        public int maxAllocationSize;

        /**
         * Max allocation count MemGuard can detect its under/overflow issues.
         */
        @Keep
        public int maxDetectableAllocationCount;

        /**
         * Max skipped allocation count between two guarded allocations.
         * For example, if 5 was set to this option, MemGuard will generate a random number 'k' in range [0,5] and
         * the first k-th allocations will be ignored.
         */
        @Keep
        public int maxSkippedAllocationCount;

        /**
         * Probability of putting guard page on the left side of specific pointer.
         * For example, if 30 was set to this option, the probability of a pointer being guarded on the left side
         * will be 30%, and the probability of a pointer being guarded on the right side will be 70%.
         */
        @Keep
        public int percentageOfLeftSideGuard;

        /**
         * Whether MemGuard should return a pointer with guard page on right side without gaps. If true was set to
         * this option, overflow issue will be easier to be detected but the returned pointer may not be aligned
         * properly. Sometimes these not aligned pointers can crash your app.
         */
        @Keep
        public boolean perfectRightSideGuard;

        /**
         * Whether MemGuard should regard overlapped reading as an issue.
         */
        @Keep
        public boolean ignoreOverlappedReading;

        /**
         * Path to write dump file when memory issue was detected. Leave it null or empty will omit dumping issue
         * info into file.
         */
        @Keep
        public String issueDumpFilePath;

        /**
         * Patterns described by RegEx of target libs that we want to detect any memory issues.
         */
        @Keep
        public String[] targetSOPatterns;

        /**
         * Patterns described by RegEx of target libs that we want to skip for detecting any memory issues.
         */
        @Keep
        public String[] ignoredSOPatterns;

        @Override
        public String toString() {
            return "Options{"
                  + "maxAllocationSize=" + maxAllocationSize
                  + ", maxDetectableAllocationCount=" + maxDetectableAllocationCount
                  + ", maxSkippedAllocationCount=" + maxSkippedAllocationCount
                  + ", percentageOfLeftSideGuard=" + percentageOfLeftSideGuard
                  + ", perfectRightSideGuard=" + perfectRightSideGuard
                  + ", ignoreOverlappedReading=" + ignoreOverlappedReading
                  + ", issueDumpFilePath=" + issueDumpFilePath
                  + ", targetSOPatterns=" + Arrays.toString(targetSOPatterns)
                  + ", ignoredSOPatterns=" + Arrays.toString(ignoredSOPatterns)
                  + '}';
        }

        private Options() {
            // Do nothing.
        }

        public static class Builder {
            private Context mContext;
            private int mMaxAllocationSize;
            private int mMaxDetectableAllocationCount;
            private int mMaxSkippedAllocationCount;
            private int mPercentageOfLeftSideGuard;
            private boolean mPerfectRightSideGuard;
            private boolean mIgnoreOverlappedReading;
            private String mIssueDumpFileDir;
            private final List<String> mTargetSOPatterns;
            private final List<String> mIgnoredSOPatterns;

            public Builder(Context context) {
                mContext = context;
                if (mContext instanceof Activity) {
                    mContext = mContext.getApplicationContext();
                }
                mMaxAllocationSize = DEFAULT_MAX_ALLOCATION_SIZE;
                mMaxDetectableAllocationCount = DEFAULT_MAX_DETECTABLE_ALLOCATION_COUNT;
                mMaxSkippedAllocationCount = DEFAULT_MAX_SKIPPED_ALLOCATION_COUNT;
                mPercentageOfLeftSideGuard = DEFAULT_PERCENTAGE_OF_LEFT_SIDE_GUARD;
                mPerfectRightSideGuard = DEFAULT_PERFECT_RIGHT_SIDE_GUARD;
                mIgnoreOverlappedReading = DEFAULT_IGNORE_OVERLAPPED_READING;
                mIssueDumpFileDir = getDefaultIssueDumpDir(context);
                mTargetSOPatterns = new ArrayList<>();
                mIgnoredSOPatterns = new ArrayList<>();
            }

            /**
             * @see Options#maxAllocationSize
             */
            public int getMaxAllocationSize() {
                return mMaxAllocationSize;
            }

            /**
             * @see Options#maxAllocationSize
             */
            public @NonNull Builder setMaxDetectableSize(int value) {
                mMaxAllocationSize = value;
                return this;
            }

            /**
             * @see Options#maxDetectableAllocationCount
             */
            public int getMaxDetectableAllocationCount() {
                return mMaxDetectableAllocationCount;
            }

            /**
             * @see Options#maxDetectableAllocationCount
             */
            public @NonNull Builder setMaxDetectableAllocationCount(int value) {
                mMaxDetectableAllocationCount = value;
                return this;
            }

            /**
             * @see Options#maxSkippedAllocationCount
             */
            public int getMaxSkippedAllocationCount() {
                return mMaxSkippedAllocationCount;
            }

            /**
             * @see Options#maxSkippedAllocationCount
             */
            public @NonNull Builder setMaxSkippedAllocationCount(int value) {
                mMaxSkippedAllocationCount = value;
                return this;
            }

            /**
             * @see Options#percentageOfLeftSideGuard
             */
            public int getPercentageOfLeftSideGuard() {
                return mPercentageOfLeftSideGuard;
            }

            /**
             * @see Options#percentageOfLeftSideGuard
             */
            public @NonNull Builder setPercentageOfLeftSideGuard(int value) {
                mPercentageOfLeftSideGuard = value;
                return this;
            }

            /**
             * @see Options#perfectRightSideGuard
             */
            public boolean isPerfectRightSideGuard() {
                return mPerfectRightSideGuard;
            }

            /**
             * @see Options#perfectRightSideGuard
             */
            public @NonNull Builder setIsPerfectRightSideGuard(boolean value) {
                mPerfectRightSideGuard = value;
                return this;
            }

            /**
             * @see Options#ignoreOverlappedReading
             */
            public boolean isIgnoreOverlappedReading() {
                return mIgnoreOverlappedReading;
            }

            /**
             * @see Options#ignoreOverlappedReading
             */
            public @NonNull Builder setIsIgnoreOverlappedReading(boolean value) {
                mIgnoreOverlappedReading = value;
                return this;
            }

            /**
             * @see Options#issueDumpFilePath
             */
            public @Nullable String getIssueDumpFileDir() {
                return mIssueDumpFileDir;
            }

            /**
             * @see Options#issueDumpFilePath
             */
            public @NonNull Builder setIssueDumpFileDir(@Nullable String value) {
                mIssueDumpFileDir = value;
                return this;
            }

            /**
             * @see Options#targetSOPatterns
             */
            public @NonNull List<String> getTargetSOPatterns() {
                return Collections.unmodifiableList(mTargetSOPatterns);
            }

            /**
             * @see Options#targetSOPatterns
             */
            public @NonNull Builder setTargetSOPattern(@NonNull String value, String... nextValues) {
                mTargetSOPatterns.clear();
                mTargetSOPatterns.add(value);
                mTargetSOPatterns.addAll(Arrays.asList(nextValues));
                return this;
            }

            /**
             * @see Options#ignoredSOPatterns
             */
            public @NonNull List<String> getIgnoredSOPatterns() {
                return Collections.unmodifiableList(mIgnoredSOPatterns);
            }

            /**
             * @see Options#ignoredSOPatterns
             */
            public @NonNull Builder setIgnoredSOPattern(@NonNull String value, String... nextValues) {
                mIgnoredSOPatterns.clear();
                mIgnoredSOPatterns.add(value);
                mIgnoredSOPatterns.addAll(Arrays.asList(nextValues));
                return this;
            }

            public @NonNull Options build() {
                final Options res = new Options();
                if (getTargetSOPatterns().isEmpty()) {
                    setTargetSOPattern(DEFAULT_TARGET_SO_PATTERN);
                }
                res.maxAllocationSize = getMaxAllocationSize();
                res.maxDetectableAllocationCount = getMaxDetectableAllocationCount();
                res.maxSkippedAllocationCount = getMaxSkippedAllocationCount();
                res.percentageOfLeftSideGuard = getPercentageOfLeftSideGuard();
                res.perfectRightSideGuard = isPerfectRightSideGuard();
                res.ignoreOverlappedReading = isIgnoreOverlappedReading();
                res.issueDumpFilePath = generateIssueDumpFilePath(mContext, getIssueDumpFileDir());
                res.targetSOPatterns = getTargetSOPatterns().toArray(new String[0]);
                res.ignoredSOPatterns = getIgnoredSOPatterns().toArray(new String[0]);
                return res;
            }
        }
    }

    private static String getDefaultIssueDumpDir(@NonNull Context context) {
        final File result = new File(context.getCacheDir(), "memguard");
        return result.getAbsolutePath();
    }

    private static String getProcessSuffix(@NonNull Context context) {
        final ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        final List<ActivityManager.RunningAppProcessInfo> runningProcs = am.getRunningAppProcesses();
        final int myUid = Process.myUid();
        final int myPid = Process.myPid();
        if (runningProcs != null) {
            for (ActivityManager.RunningAppProcessInfo procInfo : runningProcs) {
                if (procInfo.uid == myUid && procInfo.pid == myPid) {
                    final int colIdx = procInfo.processName.lastIndexOf(':');
                    if (colIdx >= 0) {
                        return procInfo.processName.substring(colIdx + 1);
                    } else {
                        return "main";
                    }
                }
            }
        }
        return "@";
    }

    private static String generateIssueDumpFilePath(Context context, String dirPath) {
        return new File(dirPath, "memguard_issue_in_proc_"
                + getProcessSuffix(context) + "_" + Process.myPid() + ".txt").getAbsolutePath();
    }
}
