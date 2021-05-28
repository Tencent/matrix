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

package com.tencent.components.backtrace;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.OperationCanceledException;
import android.os.Process;
import android.system.ErrnoException;
import android.system.Os;
import android.system.StructStat;

import com.tencent.components.backtrace.WarmUpScheduler.TaskType;
import com.tencent.matrix.util.MatrixLog;

import java.io.File;
import java.io.FileFilter;
import java.io.IOException;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.Queue;
import java.util.concurrent.CancellationException;

import static com.tencent.components.backtrace.WarmUpService.ARGS_WARM_UP_ELF_START_OFFSET;
import static com.tencent.components.backtrace.WarmUpService.ARGS_WARM_UP_PATH_OF_ELF;
import static com.tencent.components.backtrace.WarmUpService.ARGS_WARM_UP_SAVING_PATH;
import static com.tencent.components.backtrace.WarmUpService.BIND_ARGS_ENABLE_LOGGER;
import static com.tencent.components.backtrace.WarmUpService.BIND_ARGS_PATH_OF_XLOG_SO;
import static com.tencent.components.backtrace.WarmUpService.CMD_WARM_UP_SINGLE_ELF_FILE;
import static com.tencent.components.backtrace.WarmUpService.OK;
import static com.tencent.components.backtrace.WarmUpService.RESULT_OF_WARM_UP;
import static com.tencent.components.backtrace.WarmUpUtility.DURATION_CLEAN_UP_EXPIRED;
import static com.tencent.components.backtrace.WarmUpUtility.DURATION_LAST_ACCESS_EXPIRED;
import static com.tencent.components.backtrace.WarmUpUtility.iterateTargetDirectory;

class WarmUpDelegate {

    private final static String TAG = "Matrix.WarmUpDelegate";

    private final static String ACTION_WARMED_UP = "action.backtrace.warmed-up";
    private final static String PERMISSION_WARMED_UP = ".backtrace.warmed_up";

    private final static String TASK_TAG_WARM_UP = "warm-up";
    private final static String TASK_TAG_CLEAN_UP = "clean-up";
    private final static String TASK_TAG_CONSUMING_UP = "consuming-up";
    private final static String TASK_TAG_COMPUTE_DISK_USAGE = "compute-disk-usage";

    private boolean mIsolateRemote = false;
    String mSavingPath;
    private WarmedUpReceiver mWarmedUpReceiver;
    private ThreadTaskExecutor mThreadTaskExecutor;
    private WarmUpScheduler mWarmUpScheduler;
    private WeChatBacktrace.Configuration mConfiguration;

    private final boolean[] mPrepared = {false};

    static volatile WarmUpReporter sReporter;

    void prepare(WeChatBacktrace.Configuration configuration) {

        synchronized (mPrepared) {
            if (mPrepared[0]) {
                return;
            }
            mPrepared[0] = true;
        }

        mConfiguration = configuration;
        mIsolateRemote = configuration.mWarmUpInIsolateProcess;
        mThreadTaskExecutor = new ThreadTaskExecutor("WeChatBacktraceTask");
        mWarmUpScheduler = new WarmUpScheduler(this, configuration.mContext,
                configuration.mWarmUpTiming, configuration.mWarmUpDelay);

        if (configuration.mIsWarmUpProcess) {

            Context context = configuration.mContext;

            if (!WarmUpUtility.hasWarmedUp(context)) {
                MatrixLog.i(TAG, "Has not been warmed up");
                mWarmUpScheduler.scheduleTask(TaskType.WarmUp);
            }

            if (WarmUpUtility.needCleanUp(context)) {
                MatrixLog.i(TAG, "Need clean up");
                mWarmUpScheduler.scheduleTask(TaskType.CleanUp);
            }

            if (WarmUpUtility.shouldComputeDiskUsage(context)) {
                MatrixLog.i(TAG, "Should schedule disk usage task.");
                mWarmUpScheduler.scheduleTask(TaskType.DiskUsage);
            }
        }
    }

    void requestConsuming() {
        if (!WarmUpUtility.hasWarmedUp(mConfiguration.mContext)) {
            return;
        }

        mWarmUpScheduler.scheduleTask(TaskType.RequestConsuming);
    }

    boolean isBacktraceThreadBlocked() {
        if (mThreadTaskExecutor != null) {
            return mThreadTaskExecutor.isThreadBlocked();
        }
        return true;
    }

    public void setSavingPath(String savingPath) {
        mSavingPath = savingPath;
        WeChatBacktraceNative.setSavingPath(savingPath);
    }

    static boolean internalWarmUpSoPath(String pathOfSo, int elfStartOffset, boolean onlySaveFile) {
        return WeChatBacktraceNative.warmUp(pathOfSo, elfStartOffset, onlySaveFile);
    }

    private final static class RemoteWarmUpInvoker
            implements WarmUpInvoker, WarmUpService.RemoteConnection {

        WarmUpService.RemoteInvokerImpl mImpl = new WarmUpService.RemoteInvokerImpl();

        private final String mSavingPath;
        private Context mContext;
        private Bundle mArgs;

        RemoteWarmUpInvoker(String savingPath) {
            mSavingPath = savingPath;
        }

        @Override
        public boolean isConnected() {
            return mImpl.isConnected();
        }

        @Override
        public boolean connect(Context context, Bundle args) {
            mContext = context;
            mArgs = args;
            return mImpl.connect(context, args);
        }

        @Override
        public void disconnect(Context context) {
            mImpl.disconnect(context);
        }

        @Override
        public boolean warmUp(String pathOfElf, int offset) {
            if (!isConnected()) {
                connect(mContext, mArgs);
            }
            Bundle args = new Bundle();
            args.putString(ARGS_WARM_UP_SAVING_PATH, mSavingPath);
            args.putString(ARGS_WARM_UP_PATH_OF_ELF, pathOfElf);
            args.putInt(ARGS_WARM_UP_ELF_START_OFFSET, offset);
            Bundle result = mImpl.call(CMD_WARM_UP_SINGLE_ELF_FILE, args);
            int retCode = result != null ? result.getInt(RESULT_OF_WARM_UP) : -100;
            boolean ret = retCode == OK;
            if (ret) {
                WeChatBacktraceNative.notifyWarmedUp(pathOfElf, offset);
            }
            MatrixLog.i(TAG, "Warm-up %s:%s - retCode %s", pathOfElf, offset, retCode);
            return ret;
        }
    }

    private final static class LocalWarmUpInvoker implements WarmUpInvoker {
        @Override
        public boolean warmUp(String pathOfElf, int offset) {
            return internalWarmUpSoPath(pathOfElf, offset, false);
        }
    }

    private WarmUpInvoker acquireWarmUpInvoker() {
        if (mIsolateRemote) {
            RemoteWarmUpInvoker invoker = new RemoteWarmUpInvoker(mSavingPath);

            Bundle args = new Bundle();
            args.putBoolean(BIND_ARGS_ENABLE_LOGGER, mConfiguration.mEnableIsolateProcessLog);
            args.putString(BIND_ARGS_PATH_OF_XLOG_SO, mConfiguration.mPathOfXLogSo);
            if (invoker.connect(mConfiguration.mContext, args)) {
                return invoker;
            }
            return null;
        } else {
            return new LocalWarmUpInvoker();
        }
    }

    private void releaseWarmUpInvoker(WarmUpInvoker invoker) {
        if (mIsolateRemote) {
            ((RemoteWarmUpInvoker) invoker).disconnect(mConfiguration.mContext);
        }
    }

    synchronized void registerWarmedUpReceiver(WeChatBacktrace.Configuration configuration,
                                               WeChatBacktrace.Mode mode) {

        if (WarmUpUtility.hasWarmedUp(configuration.mContext)) {
            return;
        }

        if (mWarmedUpReceiver == null) {
            mWarmedUpReceiver = new WarmedUpReceiver(mode);
        } else {
            return;
        }

        MatrixLog.i(TAG, "Register warm-up receiver.");

        IntentFilter filter = new IntentFilter();
        filter.addAction(ACTION_WARMED_UP);
        configuration.mContext.registerReceiver(mWarmedUpReceiver, filter,
                configuration.mContext.getPackageName() + PERMISSION_WARMED_UP, null);
    }

    private void broadcastWarmedUp(Context context) {
        try {
            File warmedUpFile = WarmUpUtility.warmUpMarkedFile(context);
            warmedUpFile.createNewFile();
            WarmUpUtility.writeContentToFile(warmedUpFile, context.getApplicationInfo().nativeLibraryDir);
        } catch (IOException e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }


        WeChatBacktraceNative.setWarmedUp(true);
        updateBacktraceMode(mConfiguration.mBacktraceMode);

        MatrixLog.i(TAG, "Broadcast warmed up message to other processes.");

        Intent intent = new Intent(ACTION_WARMED_UP);
        intent.putExtra("pid", Process.myPid());
        context.sendBroadcast(intent, context.getPackageName() + PERMISSION_WARMED_UP);

        WarmUpReporter callback = WarmUpDelegate.sReporter;
        if (callback != null) {
            callback.onReport(WarmUpReporter.ReportEvent.WarmedUp);
        }
    }

    void warmingUp(final CancellationSignal cs) {

        mThreadTaskExecutor.arrangeTask(new Runnable() {
            @Override
            public void run() {

                MatrixLog.i(TAG, "Going to warm up.");
                boolean cancelled = false;
                WarmUpInvoker invoker = null;
                try {

                    if (!new File(WarmUpUtility.validateSavingPath(mConfiguration)).isDirectory()) {
                        MatrixLog.w(TAG, "Saving path is not a directory.");
                        mWarmUpScheduler.taskFinished(TaskType.WarmUp);
                        return;
                    }

                    invoker = acquireWarmUpInvoker();

                    if (invoker == null) {
                        MatrixLog.w(TAG, "Failed to acquire warm-up invoker");
                        return;
                    }

                    final WarmUpInvoker invokerFinal = invoker;

                    for (String directory : mConfiguration.mWarmUpDirectoriesList) {
                        File dir = new File(directory);
                        iterateTargetDirectory(dir, cs, new FileFilter() {
                            @Override
                            public boolean accept(File file) {
                                String absolutePath = file.getAbsolutePath();
                                int offset = 0;
                                if (file.exists() &&
                                        !warmUpBlocked(absolutePath, offset) &&
                                        (absolutePath.endsWith(".so") ||
                                                absolutePath.endsWith(".odex") ||
                                                absolutePath.endsWith(".oat") ||
                                                absolutePath.endsWith(".dex") // maybe
                                        )) {
                                    MatrixLog.i(TAG, "Warming up so %s", absolutePath);
                                    boolean ret = invokerFinal.warmUp(absolutePath, offset);
                                    if (!ret) {
                                        warmUpFailed(absolutePath, offset);
                                    }
                                }
                                return false;
                            }
                        });
                    }
                } catch (OperationCanceledException exception) {
                    cancelled = true;
                } catch (Throwable t) {
                    MatrixLog.printErrStackTrace(TAG, t, "");
                } finally {
                    if (invoker != null) {
                        releaseWarmUpInvoker(invoker);
                    }
                }

                if (!cancelled) {
                    mWarmUpScheduler.taskFinished(TaskType.WarmUp);
                    broadcastWarmedUp(mConfiguration.mContext);
                    MatrixLog.i(TAG, "Warm-up done.");
                } else {
                    MatrixLog.i(TAG, "Warm-up cancelled.");
                }
            }
        }, TASK_TAG_WARM_UP);
    }

    void cleaningUp(final CancellationSignal cs) {
        mThreadTaskExecutor.arrangeTask(new Runnable() {
            @Override
            public void run() {
                File savingDir = new File(WarmUpUtility.validateSavingPath(mConfiguration));

                MatrixLog.i(TAG, "Going to clean up saving path(%s)..", savingDir.getAbsoluteFile());

                if (!savingDir.isDirectory()) {
                    mWarmUpScheduler.taskFinished(TaskType.CleanUp);
                    return;
                }

                boolean cancelled = false;
                try {
                    iterateTargetDirectory(savingDir, cs, new FileFilter() {
                        @Override
                        public boolean accept(File pathname) {
                            if (pathname.getName().contains("_malformed_") || pathname.getName().contains("_temp_")) {
                                if (System.currentTimeMillis() - pathname.lastModified() >= DURATION_CLEAN_UP_EXPIRED) {
                                    MatrixLog.i(TAG, "Delete malformed and temp file %s", pathname.getAbsolutePath());
                                    pathname.delete();
                                }
                            } else {
                                try {
                                    StructStat stat = Os.lstat(pathname.getAbsolutePath());
                                    long lastAccessTime = Math.max(stat.st_atime, stat.st_mtime) * 1000L;
                                    MatrixLog.i(TAG, "File(%s) last access time %s", pathname.getAbsolutePath(), lastAccessTime);
                                    if ((System.currentTimeMillis() - lastAccessTime) > DURATION_LAST_ACCESS_EXPIRED) {
                                        pathname.delete();
                                        MatrixLog.i(TAG, "Delete long time no access file(%s)", pathname.getAbsolutePath());
                                    }
                                } catch (ErrnoException e) {
                                    MatrixLog.printErrStackTrace(TAG, e, "");
                                }
                            }
                            return false;
                        }
                    });
                } catch (OperationCanceledException exception) {
                    cancelled = true;
                } catch (Throwable t) {
                    MatrixLog.printErrStackTrace(TAG, t, "");
                }

                if (!cancelled) {
                    WarmUpUtility.markCleanUpTimestamp(mConfiguration.mContext);
                    mWarmUpScheduler.taskFinished(TaskType.CleanUp);
                    MatrixLog.i(TAG, "Clean up saving path(%s) done.", savingDir.getAbsoluteFile());

                    WarmUpReporter callback = WarmUpDelegate.sReporter;
                    if (callback != null) {
                        callback.onReport(WarmUpReporter.ReportEvent.CleanedUp);
                    }
                } else {
                    MatrixLog.i(TAG, "Clean up saving path(%s) cancelled.", savingDir.getAbsoluteFile());
                }
            }
        }, TASK_TAG_CLEAN_UP);
    }

    void consumingRequestedQut(final CancellationSignal cs) {
        mThreadTaskExecutor.arrangeTask(new Runnable() {
            @Override
            public void run() {
                MatrixLog.i(TAG, "Going to consume requested QUT.");

                String[] arrayOfRequesting = WeChatBacktraceNative.consumeRequestedQut();

                WarmUpInvoker invoker = acquireWarmUpInvoker();

                if (invoker == null) {
                    mWarmUpScheduler.taskFinished(TaskType.RequestConsuming);
                    MatrixLog.w(TAG, "Failed to acquire warm-up invoker.");
                    return;
                }

                try {
                    for (String path : arrayOfRequesting) {
                        int index = path.lastIndexOf(':');
                        String pathOfElf = path;
                        int offset = 0;
                        if (index != -1) {
                            try {
                                pathOfElf = path.substring(0, index);
                                offset = Integer.valueOf(path.substring(index + 1));
                            } catch (Throwable ignore) {
                            }
                        }

                        if (!warmUpBlocked(pathOfElf, offset)) {
                            boolean ret = invoker.warmUp(pathOfElf, offset);
                            if (!ret) {
                                warmUpFailed(pathOfElf, offset);
                            }
                        }

                        MatrixLog.i(TAG, "Consumed requested QUT -> %s", path);

                        if (cs != null && cs.isCanceled()) {
                            MatrixLog.i(TAG, "Consume requested QUT canceled.");
                            break;
                        }
                    }
                    MatrixLog.i(TAG, "Consume requested QUT done.");
                } finally {
                    releaseWarmUpInvoker(invoker);
                    mWarmUpScheduler.taskFinished(TaskType.RequestConsuming);
                }
            }
        }, TASK_TAG_CONSUMING_UP);
    }

    void computeDiskUsage(final CancellationSignal cs) {
        mThreadTaskExecutor.arrangeTask(new Runnable() {
            @Override
            public void run() {
                File file = new File(mSavingPath);
                if (!file.isDirectory()) {
                    mWarmUpScheduler.taskFinished(TaskType.DiskUsage);
                    return;
                }

                final long[] count = new long[2];

                try {
                    iterateTargetDirectory(file, cs, new FileFilter() {
                        @Override
                        public boolean accept(File pathname) {
                            count[0] += 1;
                            count[1] += pathname.isFile() ? pathname.length() : 0;
                            return false;
                        }
                    });
                } catch (CancellationException | OperationCanceledException e) {
                    return;
                } finally {
                    mWarmUpScheduler.taskFinished(TaskType.DiskUsage);
                    WarmUpUtility.markComputeDiskUsageTimestamp(mConfiguration.mContext);

                    MatrixLog.i(TAG, "Compute disk usage, file count(%s), disk usage(%s)",
                            count[0], count[1]);
                }

                WarmUpReporter reporter = sReporter;
                if (reporter != null) {
                    reporter.onReport(WarmUpReporter.ReportEvent.DiskUsage, count[0], count[1]);
                }
            }
        }, TASK_TAG_COMPUTE_DISK_USAGE);
    }

    private boolean warmUpBlocked(String pathOfElf, int offset) {
        boolean blocked = !WarmUpUtility.UnfinishedManagement.check(mConfiguration.mContext, pathOfElf, offset);
        if (blocked) {
            MatrixLog.w(TAG, "Elf file %s:%s has blocked and will not do warm-up.", pathOfElf, offset);
        }

        return blocked;
    }

    private void warmUpFailed(String pathOfElf, int offset) {
        WarmUpReporter reporter = sReporter;
        if (reporter != null) {
            reporter.onReport(WarmUpReporter.ReportEvent.WarmUpFailed, pathOfElf, offset);
        }
    }

    private final static class WarmedUpReceiver extends BroadcastReceiver {

        private WeChatBacktrace.Mode mCurrentBacktraceMode;

        WarmedUpReceiver(WeChatBacktrace.Mode mode) {
            mCurrentBacktraceMode = mode;
        }

        @Override
        public void onReceive(final Context context, final Intent intent) {

            MatrixLog.i(TAG, "Warm-up received.");

            String action = intent.getAction();
            if (action == null) return;

            switch (action) {
                case ACTION_WARMED_UP:
                    WeChatBacktraceNative.setWarmedUp(true);
                    updateBacktraceMode(mCurrentBacktraceMode);
                    try {
                        context.unregisterReceiver(this);
                    } catch (Throwable e) {
                        MatrixLog.printErrStackTrace(TAG, e, "Unregister receiver twice.");
                    }
                    break;
            }
        }
    }

    private static void updateBacktraceMode(WeChatBacktrace.Mode current) {
        if (current == WeChatBacktrace.Mode.FpUntilQuickenWarmedUp ||
                current == WeChatBacktrace.Mode.DwarfUntilQuickenWarmedUp) {
            WeChatBacktraceNative.setBacktraceMode(WeChatBacktrace.Mode.Quicken.value);
        }
    }

    final static class ThreadTaskExecutor implements Runnable, Handler.Callback {
        private String mThreadName;
        private Thread mThreadExecutor;
        private HashMap<String, Runnable> mRunnableTasks = new HashMap<>();
        private Queue<String> mTaskQueue = new LinkedList<>();

        private Handler mBlockedChecker = new Handler(Looper.getMainLooper(), this);
        private final static int MSG_BLOCKED_CHECK = 1;
        private final static long BLOCKED_CHECK_INTERVAL = 300 * 1000;

        private boolean mThreadBlocked = false;

        public ThreadTaskExecutor(String threadName) {
            mThreadName = threadName;
        }

        public boolean isThreadBlocked() {
            return mThreadBlocked;
        }

        public void arrangeTask(Runnable runnable, String tag) {
            synchronized (mTaskQueue) {
                if (mTaskQueue.contains(tag)) {
                    return;
                }
                mTaskQueue.add(tag);
                mRunnableTasks.put(tag, runnable);
            }

            synchronized (this) {
                if (mThreadExecutor == null || !mThreadExecutor.isAlive()) {
                    mThreadExecutor = new Thread(this, mThreadName);
                    mThreadExecutor.setPriority(Thread.NORM_PRIORITY);
                    mThreadExecutor.start();
                    mBlockedChecker.removeMessages(MSG_BLOCKED_CHECK);
                    mBlockedChecker.sendEmptyMessageDelayed(MSG_BLOCKED_CHECK, BLOCKED_CHECK_INTERVAL);
                }
            }
        }

        volatile long[] mTaskStartTS = {0};

        @Override
        public void run() {

            mThreadBlocked = false;

            synchronized (mTaskStartTS) {
                mTaskStartTS[0] = System.currentTimeMillis();
            }

            try {
                Runnable runnable = null;
                String tag = null;
                do {

                    if (runnable != null) {
                        long start = System.currentTimeMillis();
                        MatrixLog.i(TAG, "Before '%s' task execution..", tag);
                        runnable.run();
                        MatrixLog.i(TAG, "After '%s' task execution..", tag);

                        long duration = System.currentTimeMillis() - start;
                        WarmUpReporter callback = WarmUpDelegate.sReporter;
                        if (callback != null) {
                            if (TASK_TAG_WARM_UP.equalsIgnoreCase(tag)) {
                                callback.onReport(WarmUpReporter.ReportEvent.WarmUpDuration, duration);
                            } else if (TASK_TAG_CONSUMING_UP.equalsIgnoreCase(tag)) {
                                callback.onReport(WarmUpReporter.ReportEvent.ConsumeRequestDuration, duration);
                            }
                        }
                    }

                    synchronized (mTaskQueue) {
                        tag = mTaskQueue.poll();
                        if (tag == null) {
                            return;
                        }
                        runnable = mRunnableTasks.remove(tag);
                    }
                } while (runnable != null);
            } finally {
                synchronized (mTaskStartTS) {
                    mTaskStartTS[0] = 0;
                }

                mBlockedChecker.removeMessages(MSG_BLOCKED_CHECK);
            }

        }

        @Override
        public boolean handleMessage(Message msg) {
            if (msg.what == MSG_BLOCKED_CHECK) {
                synchronized (mTaskStartTS) {
                    if (mTaskStartTS[0] == 0) {
                        return false;
                    }
                }

                mThreadBlocked = true;

                WarmUpReporter callback = WarmUpDelegate.sReporter;
                if (callback != null) {
                    callback.onReport(WarmUpReporter.ReportEvent.WarmUpThreadBlocked);
                }
            }
            return false;
        }
    }
}
