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
import com.tencent.stubs.logger.Log;

import java.io.File;
import java.io.FileFilter;
import java.io.IOException;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.Queue;

import static com.tencent.components.backtrace.WarmUpService.ARGS_WARM_UP_ELF_START_OFFSET;
import static com.tencent.components.backtrace.WarmUpService.ARGS_WARM_UP_PATH_OF_ELF;
import static com.tencent.components.backtrace.WarmUpService.ARGS_WARM_UP_SAVING_PATH;
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

    private boolean mIsolateRemote = false;
    private String mSavingPath;
    private WarmedUpReceiver mWarmedUpReceiver;
    private ThreadTaskExecutor mThreadTaskExecutor;
    private WarmUpScheduler mWarmUpScheduler;
    private WeChatBacktrace.Configuration mConfiguration;

    private boolean[] mPrepared = {false};

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
                Log.i(TAG, "Has not been warmed up");
                mWarmUpScheduler.scheduleTask(TaskType.WarmUp);
            }

            if (WarmUpUtility.needCleanUp(context)) {
                Log.i(TAG, "Need clean up");
                mWarmUpScheduler.scheduleTask(TaskType.CleanUp);
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

    static void internalWarmUpSoPath(String pathOfSo, int elfStartOffset) {
        WeChatBacktraceNative.warmUp(pathOfSo, elfStartOffset);
    }

    private final static class RemoteWarmUpInvoker
            implements WarmUpInvoker, WarmUpService.RemoteConnection {

        WarmUpService.RemoteInvokerImpl mImpl = new WarmUpService.RemoteInvokerImpl();

        private String mSavingPath;

        RemoteWarmUpInvoker(String savingPath) {
            mSavingPath = savingPath;
        }

        @Override
        public boolean connect(Context context) {
            return mImpl.connect(context);
        }

        @Override
        public void disconnect(Context context) {
            mImpl.disconnect(context);
        }

        @Override
        public boolean warmUp(String pathOfElf, int offset) {
            Bundle args = new Bundle();
            args.putString(ARGS_WARM_UP_SAVING_PATH, mSavingPath);
            args.putString(ARGS_WARM_UP_PATH_OF_ELF, pathOfElf);
            args.putLong(ARGS_WARM_UP_ELF_START_OFFSET, offset);
            Bundle result = mImpl.call(CMD_WARM_UP_SINGLE_ELF_FILE, args);
            boolean ret = result != null && result.getInt(RESULT_OF_WARM_UP) == OK;
            if (ret) {
                WeChatBacktraceNative.notifyWarmedUp(pathOfElf, offset);
            }
            Log.i(TAG, "Warm-up %s:%s - ret %s", pathOfElf, offset, ret);
            return ret;
        }
    }

    private final static class LocalWarmUpInvoker implements WarmUpInvoker {
        @Override
        public boolean warmUp(String pathOfElf, int offset) {
            internalWarmUpSoPath(pathOfElf, offset);
            return true;
        }
    }

    private WarmUpInvoker acquireWarmUpInvoker() {
        if (mIsolateRemote) {
            RemoteWarmUpInvoker invoker = new RemoteWarmUpInvoker(mSavingPath);
            if (invoker.connect(mConfiguration.mContext)) {
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

        Log.i(TAG, "Register warm-up receiver.");

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
            Log.printStack(Log.ERROR, TAG, e);
        }


        WeChatBacktraceNative.setWarmedUp(true);
        updateBacktraceMode(mConfiguration.mBacktraceMode);

        Log.i(TAG, "Broadcast warmed up message to other processes.");

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

                Log.i(TAG, "Going to warm up.");
                boolean cancelled = false;
                WarmUpInvoker invoker = null;
                try {

                    if (!new File(WarmUpUtility.validateSavingPath(mConfiguration)).isDirectory()) {
                        Log.w(TAG, "Saving path is not a directory.");
                        mWarmUpScheduler.taskFinished(TaskType.WarmUp);
                        return;
                    }

                    invoker = acquireWarmUpInvoker();

                    if (invoker == null) {
                        Log.w(TAG, "Failed to acquire warm-up invoker");
                        return;
                    }

                    final WarmUpInvoker invokerFinal = invoker;

                    for (String directory : mConfiguration.mWarmUpDirectoriesList) {
                        File dir = new File(directory);
                        iterateTargetDirectory(dir, cs, new FileFilter() {
                            @Override
                            public boolean accept(File file) {
                                String absolutePath = file.getAbsolutePath();
                                if (file.exists() &&
                                        (absolutePath.endsWith(".so") ||
                                                absolutePath.endsWith(".odex") ||
                                                absolutePath.endsWith(".oat")
                                        )) {
                                    Log.i(TAG, "Warming up so %s", absolutePath);
                                    invokerFinal.warmUp(absolutePath, 0);
                                }
                                return false;
                            }
                        });
                    }
                } catch (OperationCanceledException exception) {
                    cancelled = true;
                } catch (Throwable t) {
                    Log.printStack(Log.ERROR, TAG, t);
                } finally {
                    if (invoker != null) {
                        releaseWarmUpInvoker(invoker);
                    }
                }

                if (!cancelled) {
                    mWarmUpScheduler.taskFinished(TaskType.WarmUp);
                    broadcastWarmedUp(mConfiguration.mContext);
                    Log.i(TAG, "Warm-up done.");
                } else {
                    Log.i(TAG, "Warm-up cancelled.");
                }
            }
        }, "warm-up");
    }

    void cleaningUp(final CancellationSignal cs) {
        mThreadTaskExecutor.arrangeTask(new Runnable() {
            @Override
            public void run() {
                File savingDir = new File(WarmUpUtility.validateSavingPath(mConfiguration));

                Log.i(TAG, "Going to clean up saving path(%s)..", savingDir.getAbsoluteFile());

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
                                    Log.i(TAG, "Delete malformed and temp file %s", pathname.getAbsolutePath());
                                    pathname.delete();
                                }
                            } else {
                                try {
                                    StructStat stat = Os.lstat(pathname.getAbsolutePath());
                                    long lastAccessTime = Math.max(stat.st_atime, stat.st_mtime) * 1000L;
                                    Log.i(TAG, "File(%s) last access time %s", pathname.getAbsolutePath(), lastAccessTime);
                                    if ((System.currentTimeMillis() - lastAccessTime) > DURATION_LAST_ACCESS_EXPIRED) {
                                        pathname.delete();
                                        Log.i(TAG, "Delete long time no access file(%s)", pathname.getAbsolutePath());
                                    }
                                } catch (ErrnoException e) {
                                    Log.printStack(Log.ERROR, TAG, e);
                                }
                            }
                            return false;
                        }
                    });
                } catch (OperationCanceledException exception) {
                    cancelled = true;
                } catch (Throwable t) {
                    Log.printStack(Log.ERROR, TAG, t);
                }

                if (!cancelled) {
                    WarmUpUtility.markCleanUpTimestamp(mConfiguration.mContext);
                    mWarmUpScheduler.taskFinished(TaskType.CleanUp);
                    Log.i(TAG, "Clean up saving path(%s) done.", savingDir.getAbsoluteFile());

                    WarmUpReporter callback = WarmUpDelegate.sReporter;
                    if (callback != null) {
                        callback.onReport(WarmUpReporter.ReportEvent.CleanedUp);
                    }
                } else {
                    Log.i(TAG, "Clean up saving path(%s) cancelled.", savingDir.getAbsoluteFile());
                }
            }
        }, "clean-up");
    }

    void consumingRequestedQut(CancellationSignal cs) {
        mThreadTaskExecutor.arrangeTask(new Runnable() {
            @Override
            public void run() {
                Log.i(TAG, "Going to consume requested QUT.");

                String[] arrayOfRequesting = WeChatBacktraceNative.consumeRequestedQut();

                WarmUpInvoker invoker = acquireWarmUpInvoker();

                if (invoker == null) {
                    Log.w(TAG, "Failed to acquire warm-up invoker.");
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

                        invoker.warmUp(pathOfElf, offset);

                        Log.i(TAG, "Consumed requested QUT -> %s", path);
                    }
                    Log.i(TAG, "Consume requested QUT done.");
                } finally {
                    releaseWarmUpInvoker(invoker);
                    mWarmUpScheduler.taskFinished(TaskType.RequestConsuming);
                }
            }
        }, "consuming-up");
    }

    private final static class WarmedUpReceiver extends BroadcastReceiver {

        private WeChatBacktrace.Mode mCurrentBacktraceMode;

        WarmedUpReceiver(WeChatBacktrace.Mode mode) {
            mCurrentBacktraceMode = mode;
        }

        @Override
        public void onReceive(Context context, final Intent intent) {

            Log.i(TAG, "Warm-up received.");

            String action = intent.getAction();
            if (action == null) return;

            switch (action) {
                case ACTION_WARMED_UP:
                    WeChatBacktraceNative.setWarmedUp(true);
                    updateBacktraceMode(mCurrentBacktraceMode);
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
        private final static long BLOCKED_CHECK_INTERVAL = 600 * 1000;

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

        @Override
        public void run() {

            mThreadBlocked = false;

            Runnable runnable = null;
            String tag = null;
            do {

                if (runnable != null) {
                    Log.i(TAG, "Before '%s' task execution..", tag);
                    runnable.run();
                    Log.i(TAG, "After '%s' task execution..", tag);
                }

                synchronized (mTaskQueue) {
                    tag = mTaskQueue.poll();
                    if (tag == null) {
                        return;
                    }
                    runnable = mRunnableTasks.remove(tag);
                }
            } while (runnable != null);

            mBlockedChecker.removeMessages(MSG_BLOCKED_CHECK);
        }

        @Override
        public boolean handleMessage(Message msg) {
            if (msg.what == MSG_BLOCKED_CHECK) {
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
