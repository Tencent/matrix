package com.tencent.components.backtrace;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.Build;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.OperationCanceledException;
import android.os.PowerManager;
import android.os.Process;
import android.system.ErrnoException;
import android.system.Os;

import com.tencent.stubs.logger.Log;

import java.io.File;
import java.io.FileFilter;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.HashSet;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicInteger;

public class WeChatBacktrace implements Handler.Callback {

    private final static String TAG = "Matrix.Backtrace";

    private final static String SYSTEM_LIBRARY_PATH_Q = "/apex/com.android.runtime/lib/";
    private final static String SYSTEM_LIBRARY_PATH_Q_64 = "/apex/com.android.runtime/lib64/";
    private final static String SYSTEM_LIBRARY_PATH = "/system/lib/";
    private final static String SYSTEM_LIBRARY_PATH_64 = "/system/lib64/";

    public static boolean is64BitRuntime() {
        final String currRuntimeABI = Build.CPU_ABI;
        return "arm64-v8a".equalsIgnoreCase(currRuntimeABI)
                || "x86_64".equalsIgnoreCase(currRuntimeABI)
                || "mips64".equalsIgnoreCase(currRuntimeABI);
    }

    public static String getSystemLibraryPath() {
        if (Build.VERSION.SDK_INT >= 29) {
            return !is64BitRuntime() ? SYSTEM_LIBRARY_PATH_Q : SYSTEM_LIBRARY_PATH_Q_64;
        } else {
            return !is64BitRuntime() ? SYSTEM_LIBRARY_PATH : SYSTEM_LIBRARY_PATH_64;
        }
    }

    private final static String ACTION_WARMED_UP = "action.backtrace.warmed-up";
    private final static String PERMISSION_WARMED_UP = ".backtrace.warmed_up";

    private final static String DIR_WECHAT_BACKTRACE = "wechat-backtrace";
    private final static String FILE_DEFAULT_SAVING_PATH = "saving-cache";
    private final static String FILE_WARMED_UP = "warmed-up";
    private final static String FILE_CLEAN_UP_TIMESTAMP = "clean-up.timestamp";

    private final static long DURATION_LAST_ACCESS_EXPIRED = 15L * 24 * 3600 * 1000; // milliseconds
    private final static long DURATION_CLEAN_UP_EXPIRED = 3L * 24 * 3600 * 1000; // milliseconds
    private final static long DURATION_CLEAN_UP = 7L * 24 * 3600 * 1000; // milliseconds

    private final static long DELAY_SHORTLY = 3 * 1000;
    private final static long DELAY_CLEAN_UP = DELAY_SHORTLY;
    private final static long DELAY_WARM_UP = DELAY_SHORTLY;
    private final static long DELAY_CONSUME_REQ_QUT = DELAY_SHORTLY;

    private final static int MSG_WARM_UP = 1;
    private final static int MSG_CONSUME_REQ_QUT = 2;
    private final static int MSG_CLEAN_UP = 3;

    private volatile boolean mInitialized;
    private volatile boolean mConfigured;
    private volatile Configuration mConfiguration;
    private IdleReceiver mIdleReceiver;
    private WarmedUpReceiver mWarmedUpReceiver;
    private Handler mIdleHandler;
    private ThreadTaskExecutor mThreadTaskExecutor;
    private AtomicInteger mUnfinishedTask = new AtomicInteger(0);

    @Override
    public boolean handleMessage(Message msg) {
        switch (msg.what) {
            case MSG_WARM_UP: {
                CancellationSignal cs = (CancellationSignal) msg.obj;
                warmingUp(cs);
                break;
            }
            case MSG_CONSUME_REQ_QUT: {
                CancellationSignal cs = (CancellationSignal) msg.obj;
                consumingRequestedQut(cs);
                break;
            }
            case MSG_CLEAN_UP: {
                CancellationSignal cs = (CancellationSignal) msg.obj;
                cleaningUp(cs);
                break;
            }
        }
        return false;
    }

    private final class IdleReceiver extends BroadcastReceiver {

        private boolean mIsCharging;
        private boolean mIsInteractive;
        private CancellationSignal mCancellationSignal;

        private synchronized void triggerIdle() {
            Log.i(TAG, "Idle status changed: charging = %s, interactive = %s", mIsCharging, mIsInteractive);

            if (!mIsInteractive && mCancellationSignal == null) {
                mCancellationSignal = new CancellationSignal();
                if (mConfiguration.mThisIsWarmUpProcess && !hasWarmedUp()) {
                    mIdleHandler.sendMessageDelayed(
                            Message.obtain(mIdleHandler, MSG_WARM_UP, mCancellationSignal),
                            DELAY_WARM_UP
                    );
                    Log.i(TAG, "System idle, trigger warm up in %s seconds.", (DELAY_CONSUME_REQ_QUT / 1000));
                } else {
                    mIdleHandler.sendMessageDelayed(
                            Message.obtain(mIdleHandler, MSG_CONSUME_REQ_QUT, mCancellationSignal),
                            DELAY_CONSUME_REQ_QUT
                    );
                    Log.i(TAG, "System idle, trigger consume requested qut in %s seconds.", (DELAY_CONSUME_REQ_QUT / 1000));
                }
                if (needCleanUp()) {
                    mIdleHandler.sendMessageDelayed(
                            Message.obtain(mIdleHandler, MSG_CLEAN_UP, mCancellationSignal),
                            DELAY_CONSUME_REQ_QUT
                    );
                    Log.i(TAG, "System idle, trigger clean up in %s seconds.", (DELAY_CLEAN_UP / 1000));
                }

            } else if (mIsInteractive && mCancellationSignal != null) {
                mIdleHandler.removeMessages(MSG_WARM_UP);
                mIdleHandler.removeMessages(MSG_CONSUME_REQ_QUT);
                mIdleHandler.removeMessages(MSG_CLEAN_UP);
                mCancellationSignal.cancel();
                mCancellationSignal = null;
                Log.i(TAG, "Exit idle state, task cancelled.");
            }
        }

        synchronized void refreshIdleStatus(Context context) {
            PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
            mIsInteractive = pm.isInteractive();

            IntentFilter filter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
            Intent intent = context.registerReceiver(null, filter);
            if (intent != null) {
                int status = intent.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
                mIsCharging = (status == BatteryManager.BATTERY_STATUS_CHARGING) ||
                        (status == BatteryManager.BATTERY_STATUS_FULL);
            }
            triggerIdle();
        }

        @Override
        public void onReceive(Context context, final Intent intent) {
            String action = intent.getAction();
            if (action == null) return;

            synchronized (this) {
                switch (action) {
                    case Intent.ACTION_SCREEN_ON:
                        mIsInteractive = true;
                        break;
                    case Intent.ACTION_SCREEN_OFF:
                        mIsInteractive = false;
                        break;
                    case Intent.ACTION_POWER_CONNECTED:
                        mIsCharging = true;
                        break;
                    case Intent.ACTION_POWER_DISCONNECTED:
                        mIsCharging = false;
                        break;
                }
                triggerIdle();
            }
        }
    }

    private final class WarmedUpReceiver extends BroadcastReceiver {

        @Override
        public void onReceive(Context context, final Intent intent) {

            Log.i(TAG, "Warm-up received .");

            String action = intent.getAction();
            if (action == null) return;

            switch (action) {
                case ACTION_WARMED_UP:
                    WeChatBacktraceNative.setWarmedUp(true);
                    break;
            }
        }
    }

    private final static class ThreadTaskExecutor implements Runnable {
        private String mThreadName;
        private Thread mThreadExecutor;
        private Queue<Runnable> mQueue = new ConcurrentLinkedQueue<>();

        public ThreadTaskExecutor(String threadName) {
            mThreadName = threadName;
        }

        public void arrangeTask(Runnable runnable) {
            mQueue.add(runnable);

            synchronized (this) {
                if (mThreadExecutor == null || !mThreadExecutor.isAlive()) {
                    mThreadExecutor = new Thread(this, mThreadName);
                    mThreadExecutor.setPriority(Thread.NORM_PRIORITY);
                    mThreadExecutor.start();
                }
            }
        }

        @Override
        public void run() {
            Runnable runnable;
            while ((runnable = mQueue.poll()) != null) {
                runnable.run();
            }
        }
    }

    private final static class Singleton {
        public final static WeChatBacktrace INSTANCE = new WeChatBacktrace();
    }

    public static WeChatBacktrace instance() {
        return Singleton.INSTANCE;
    }

    public boolean hasWarmedUp() {
        if (!mInitialized) {
            return false;
        }

        return warmUpMarkedFile(mConfiguration.mContext).exists();
    }

    private String readFileContent(File file) {
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

    private boolean writeContentToFile(File file, String content) {
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

    public void requestQutGenerate() {

        if (!mInitialized || !mConfigured) {
            return;
        }

        if (!hasWarmedUp()) {
            return;
        }

        mUnfinishedTask.incrementAndGet();
        registerIdleReceiver(mConfiguration.mContext);
    }

    public synchronized Configuration configure(Context context) {
        if (mConfiguration != null) {
            return mConfiguration;
        }
        mConfiguration = new Configuration(context, this);
        mInitialized = true;
        return mConfiguration;
    }

    private void iterateTargetDirectory(File target, CancellationSignal cs, FileFilter filter) {
        if (target.isDirectory()) {
            for (File file : target.listFiles()) {
                iterateTargetDirectory(file, cs, filter);
                cs.throwIfCanceled();
            }
        } else {
            filter.accept(target);
            cs.throwIfCanceled();
        }
    }

    // TODO For debug
    final CancellationSignal fakeCS = new CancellationSignal();

    private void warmingUp(final CancellationSignal cs) {

        if (fakeCS != cs) {
            return;
        }

        mThreadTaskExecutor.arrangeTask(new Runnable() {
            @Override
            public void run() {

                Log.i(TAG, "Going to warm up.");
                boolean cancelled = false;
                try {
                    if (!new File(validateSavingPath(mConfiguration)).isDirectory()) {
                        Log.i(TAG, "Saving path is not a directory.");
                    }
                    for (String directory : mConfiguration.mWarmUpDirectoriesList) {
                        File dir = new File(directory);
                        iterateTargetDirectory(dir, cs, new FileFilter() {
                            @Override
                            public boolean accept(File file) {
                                if (file.exists() && file.getAbsolutePath().endsWith(".so")) {
                                    Log.i(TAG, "Warming up so %s", file.getAbsolutePath());
                                    WeChatBacktraceNative.warmUp(file.getAbsolutePath());
                                }
                                return false;
                            }
                        });
                    }
                } catch (OperationCanceledException exception) {
                    cancelled = true;
                } catch (Throwable t) {
                    Log.printStack(Log.ERROR, TAG, t);
                }

                if (!cancelled) {
                    broadcastWarmedUp();
                    tryUnregisterIdleReceiver(mConfiguration.mContext);
                    Log.i(TAG, "Warm-up done.");
                } else {
                    Log.i(TAG, "Warm-up cancelled.");
                }
            }
        });
    }

    private void cleaningUp(final CancellationSignal cs) {
        mThreadTaskExecutor.arrangeTask(new Runnable() {
            @Override
            public void run() {
                File savingDir = new File(validateSavingPath(mConfiguration));

                Log.i(TAG, "Going to clean up saving path(%s).", savingDir.getAbsoluteFile());

                if (!savingDir.isDirectory()) {
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
                                    long lastAccessTime = Os.lstat(pathname.getAbsolutePath()).st_atime * 1000L;
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
                    markCleanUpTimestamp();
                    tryUnregisterIdleReceiver(mConfiguration.mContext);
                    Log.i(TAG, "Clean up saving path(%s) done.", savingDir.getAbsoluteFile());
                } else {
                    Log.i(TAG, "Clean up saving path(%s) cancelled.", savingDir.getAbsoluteFile());
                }
            }
        });
    }

    private void consumingRequestedQut(CancellationSignal cs) {
        mThreadTaskExecutor.arrangeTask(new Runnable() {
            @Override
            public void run() {
                Log.i(TAG, "Going to consume requested QUT.");
                WeChatBacktraceNative.consumeRequestedQut();
                Log.i(TAG, "Consume requested QUT done.");
                tryUnregisterIdleReceiver(mConfiguration.mContext);
            }
        });
    }

    private void broadcastWarmedUp() {
        try {
            File warmedUpFile = warmUpMarkedFile(mConfiguration.mContext);
            warmedUpFile.createNewFile();
            writeContentToFile(warmedUpFile, mConfiguration.mContext.getApplicationInfo().nativeLibraryDir);
        } catch (IOException e) {
            Log.printStack(Log.ERROR, TAG, e);
        }

        WeChatBacktraceNative.setWarmedUp(true);

        Log.i(TAG, "Broadcast warmed up message to other processes.");

        Intent intent = new Intent(ACTION_WARMED_UP);
        intent.putExtra("pid", Process.myPid());
        mConfiguration.mContext.sendBroadcast(intent, mConfiguration.mContext.getPackageName() + PERMISSION_WARMED_UP);
    }

    private synchronized void registerIdleReceiver(Context context) {
        if (mIdleReceiver == null) {
            mIdleReceiver = new IdleReceiver();
        } else {
            return;
        }

        Log.i(TAG, "Register idle receiver.");

        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        filter.addAction(Intent.ACTION_POWER_CONNECTED);
        filter.addAction(Intent.ACTION_POWER_DISCONNECTED);
        context.registerReceiver(mIdleReceiver, filter);
        mIdleReceiver.refreshIdleStatus(context);
    }

    private synchronized void tryUnregisterIdleReceiver(Context context) {
        if (mUnfinishedTask.getAndDecrement() > 0) {
            return;
        }
        Log.i(TAG, "Unregister idle receiver.");
        context.unregisterReceiver(mIdleReceiver);
        mIdleReceiver = null;
    }

    private synchronized void registerWarmedUpReceiver(Configuration configuration) {

        if (configuration.mThisIsWarmUpProcess) {
            return;
        }

        if (hasWarmedUp()) {
            return;
        }

        if (mWarmedUpReceiver == null) {
            mWarmedUpReceiver = new WarmedUpReceiver();
        } else {
            return;
        }

        Log.i(TAG, "Register warm-up receiver.");

        IntentFilter filter = new IntentFilter();
        filter.addAction(ACTION_WARMED_UP);
        configuration.mContext.registerReceiver(mWarmedUpReceiver, filter,
                mConfiguration.mContext.getPackageName() + PERMISSION_WARMED_UP, null);
    }

    private void markCleanUpTimestamp() {
        File timestamp = cleanUpTimestampFile(mConfiguration.mContext);
        try {
            timestamp.createNewFile();
            timestamp.setLastModified(System.currentTimeMillis());
        } catch (IOException e) {
            Log.printStack(Log.ERROR, TAG, e);
        }
    }

    private boolean needCleanUp() {
        File timestamp = cleanUpTimestampFile(mConfiguration.mContext);
        if (!timestamp.exists()) {
            return false;
        }
        return System.currentTimeMillis() - timestamp.lastModified() >= DURATION_CLEAN_UP;
    }

    private void prepareIdleScheduler(Configuration configuration) {

        mIdleHandler = new Handler(Looper.getMainLooper(), this);

        mThreadTaskExecutor = new ThreadTaskExecutor("WeChatBacktraceTask");

        mUnfinishedTask.set(0);

        if (configuration.mThisIsWarmUpProcess) {
            boolean hasWarmedUp = hasWarmedUp();
            boolean needCleanUp = needCleanUp();
            if (!hasWarmedUp) {
                mUnfinishedTask.getAndIncrement();
                Log.i(TAG, "Has not been warmed up");
            }
            if (needCleanUp) {
                mUnfinishedTask.getAndIncrement();
                Log.i(TAG, "Need clean up");
            }

            File timestamp = cleanUpTimestampFile(mConfiguration.mContext);
            if (!timestamp.exists()) {
                try {
                    timestamp.createNewFile();
                } catch (IOException e) {
                    Log.printStack(Log.ERROR, TAG, e);
                }
            }

            if (mUnfinishedTask.get() > 0) {
                registerIdleReceiver(configuration.mContext);
            }
        }
    }

    private boolean pathValidation(Configuration configuration) {
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

    private void dealWithCoolDown(Configuration configuration) {
        if (configuration.mThisIsWarmUpProcess) {
            File markFile = warmUpMarkedFile(configuration.mContext);
            if (configuration.mCoolDownIfApkUpdated && markFile.exists()) {
                String content = readFileContent(markFile);
                String lastNativeLibraryPath = content.split("\n")[0];
                if (!lastNativeLibraryPath.equalsIgnoreCase(
                        configuration.mContext.getApplicationInfo().nativeLibraryDir)) {
                    Log.i(TAG, "Apk updated, remove warmed-up file.");
                    configuration.mCoolDown = true;
                }
            }
            if (configuration.mCoolDown) {
                markFile.delete();
            }
        }
    }

    private File cleanUpTimestampFile(Context context) {
        File file = new File(context.getFilesDir().getAbsolutePath() + "/"
                + DIR_WECHAT_BACKTRACE + "/" + FILE_CLEAN_UP_TIMESTAMP);
        file.getParentFile().mkdirs();
        return file;
    }

    private File warmUpMarkedFile(Context context) {
        File file = new File(context.getFilesDir().getAbsolutePath() + "/"
                + DIR_WECHAT_BACKTRACE + "/" + FILE_WARMED_UP);
        file.getParentFile().mkdirs();
        return file;
    }

    private String defaultSavingPath(Configuration configuration) {
        return configuration.mContext.getFilesDir().getAbsolutePath() + "/"
                + DIR_WECHAT_BACKTRACE + "/" + FILE_DEFAULT_SAVING_PATH + "/";
    }

    private String validateSavingPath(Configuration configuration) {
        if (pathValidation(configuration)) {
            return configuration.mSavingPath;
        } else {
            return defaultSavingPath(configuration);
        }
    }

    private void configure(Configuration configuration) {

        // Init saving path
        String savingPath = validateSavingPath(configuration);
        Log.i(TAG, "Set saving path = %s", savingPath);
        File file = new File(savingPath);
        file.mkdirs();
        if (!savingPath.endsWith(File.separator)) {
            savingPath += File.separator;
        }
        WeChatBacktraceNative.setSavingPath(savingPath);

        // Remove warm up marked file if cool down is set.
        dealWithCoolDown(configuration);

        // Register scheduler
        prepareIdleScheduler(configuration);

        // Set warmed up flag
        WeChatBacktraceNative.setWarmedUp(hasWarmedUp());

        // Register warmed up receiver for other processes.
        registerWarmedUpReceiver(configuration);

        mConfigured = true;

        if (configuration.mThisIsWarmUpProcess && !hasWarmedUp()) {
            mIdleHandler.sendMessageDelayed(
                    Message.obtain(mIdleHandler, MSG_WARM_UP, fakeCS),
                    DELAY_WARM_UP * 10
            );
        }
    }

    public final static class Configuration {
        Context mContext;
        String mSavingPath;
        HashSet<String> mWarmUpDirectoriesList = new HashSet<>();
        boolean mCoolDown = false;
        boolean mCoolDownIfApkUpdated = true;   // Default true.
        boolean mThisIsWarmUpProcess = false;

        private boolean mCommitted = false;
        private WeChatBacktrace mWeChatBacktrace;

        Configuration(Context context, WeChatBacktrace backtrace) {
            mContext = context;
            mWeChatBacktrace = backtrace;
        }

        public Configuration savingPath(String savingPath) {
            if (mCommitted) {
                return this;
            }
            mSavingPath = savingPath;
            return this;
        }

        public Configuration directoryToWarmUp(String directory) {
            if (mCommitted) {
                return this;
            }
            mWarmUpDirectoriesList.add(directory);
            return this;
        }

        public Configuration coolDown(boolean coolDown) {
            if (mCommitted) {
                return this;
            }
            mCoolDown = coolDown;
            return this;
        }

        public Configuration coolDownIfApkUpdated(boolean ifApkUpdated) {
            if (mCommitted) {
                return this;
            }
            mCoolDownIfApkUpdated = ifApkUpdated;
            return this;
        }

        public Configuration isWarmUpProcess(boolean isWarmUpProcess) {
            if (mCommitted) {
                return this;
            }
            mThisIsWarmUpProcess = isWarmUpProcess;
            return this;
        }

        public void commit() {
            if (mCommitted) {
                return;
            }
            mCommitted = true;

            mWeChatBacktrace.configure(this);
        }

    }
}
