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

import android.content.Context;
import android.os.Build;


import com.tencent.matrix.util.MatrixLog;

import java.io.File;
import java.util.HashSet;

import static com.tencent.components.backtrace.WarmUpScheduler.DELAY_SHORTLY;

public class WeChatBacktrace {

    private final static String TAG = "Matrix.Backtrace";

    private final static String SYSTEM_LIBRARY_PATH_Q = "/apex/com.android.runtime/lib/";
    private final static String SYSTEM_LIBRARY_PATH_Q_64 = "/apex/com.android.runtime/lib64/";
    private final static String SYSTEM_LIBRARY_PATH = "/system/lib/";
    private final static String SYSTEM_LIBRARY_PATH_64 = "/system/lib64/";

    private final static String SYSTEM_BOOT_OAT_PATH = "/system/framework/arm/";
    private final static String SYSTEM_BOOT_OAT_PATH_64 = "/system/framework/arm64/";

    public final static String ISOLATE_PROCESS_SUFFIX = ":backtrace__";

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

    public static String getSystemFrameworkOATPath() {
        return !is64BitRuntime() ? SYSTEM_BOOT_OAT_PATH : SYSTEM_BOOT_OAT_PATH_64;
    }

    public static String getBaseODEXPath(Context context) {
        String abiName = !is64BitRuntime() ? "arm" : "arm64";
        return new File(
                new File(context.getApplicationInfo().nativeLibraryDir)
                        .getParentFile().getParentFile(),
                "/oat/" + abiName + "/base.odex").getAbsolutePath();
    }

    private final static String BACKTRACE_LIBRARY_NAME = "wechatbacktrace";

    private volatile boolean mInitialized;
    private volatile boolean mConfigured;
    private volatile Configuration mConfiguration;
    private WarmUpDelegate mWarmUpDelegate = new WarmUpDelegate();

    public interface LibraryLoader {
        void load(String library);
    }

    public static void setReporter(WarmUpReporter reporter) {
        WarmUpDelegate.sReporter = reporter;
    }

    private final static class Singleton {
        public final static WeChatBacktrace INSTANCE = new WeChatBacktrace();
    }

    public static WeChatBacktrace instance() {
        return Singleton.INSTANCE;
    }

    public boolean isBacktraceThreadBlocked() {
        return mWarmUpDelegate.isBacktraceThreadBlocked();
    }

    public void requestQutGenerate() {

        if (!mInitialized || !mConfigured) {
            return;
        }

        mWarmUpDelegate.requestConsuming();
    }

    public synchronized Configuration configure(Context context) {
        if (mConfiguration != null) {
            return mConfiguration;
        }
        mConfiguration = new Configuration(context, this);
        mInitialized = true;

        return mConfiguration;
    }

    public String getSavingPath() {
        return mWarmUpDelegate.mSavingPath;
    }

    private void loadLibrary(LibraryLoader loader) {
        if (loader == null) {
            loadLibrary();
        } else {
            MatrixLog.i(TAG, "Using custom library loader: %s.", loader);
            loader.load(BACKTRACE_LIBRARY_NAME);
        }
    }

    // Invoke by warm-up provider
    static void loadLibrary() {
        System.loadLibrary(BACKTRACE_LIBRARY_NAME);
    }

    static void enableLogger(String pathOfXLog, boolean enableLogger) {
        WeChatBacktraceNative.enableLogger(pathOfXLog, enableLogger);
    }

    private void dealWithCoolDown(Configuration configuration) {
        if (configuration.mIsWarmUpProcess) {
            File markFile = WarmUpUtility.warmUpMarkedFile(configuration.mContext);
            if (configuration.mCoolDownIfApkUpdated && markFile.exists()) {
                String content = WarmUpUtility.readFileContent(markFile, 4096);
                if (content == null) {
                    configuration.mCoolDown = true;
                } else {
                    String lastNativeLibraryPath = content.split("\n")[0];
                    if (!lastNativeLibraryPath.equalsIgnoreCase(
                            configuration.mContext.getApplicationInfo().nativeLibraryDir)) {
                        MatrixLog.i(TAG, "Apk updated, remove warmed-up file.");
                        configuration.mCoolDown = true;
                    }
                }
            }
            if (configuration.mCoolDown) {
                markFile.delete();

                File unfinishedFile = WarmUpUtility.unfinishedFile(configuration.mContext);
                unfinishedFile.delete();
            }
        }
    }

    public final static class ConfigurationException extends RuntimeException {
        public ConfigurationException(String message) {
            super(message);
        }
    }

    private boolean runningInIsolateProcess(Configuration configuration) {
        String processName = ProcessUtil.getProcessNameByPid(configuration.mContext);
        if (processName != null && processName.endsWith(ISOLATE_PROCESS_SUFFIX)) {
            return true;
        }
        return false;
    }

    private void configure(Configuration configuration) {

        if (runningInIsolateProcess(configuration)) {
            MatrixLog.i(TAG, "Isolate process does not need any configuration.");
            return;
        }

        if (configuration.mWarmUpInIsolateProcess && configuration.mLibraryLoader != null) {
            throw new ConfigurationException("Custom library loader is not supported in " +
                    "isolate process warm-up mode.");
        }

        // Load backtrace library.
        loadLibrary(configuration.mLibraryLoader);

        if (configuration.mEnableLog) {
            enableLogger(configuration.mPathOfXLogSo, true);
        }

        MatrixLog.i(TAG, configuration.toString());

        if (configuration.mBacktraceMode == Mode.Quicken ||
                configuration.mBacktraceMode == Mode.FpUntilQuickenWarmedUp ||
                configuration.mBacktraceMode == Mode.DwarfUntilQuickenWarmedUp
        ) {

            // Init saving path
            String savingPath = WarmUpUtility.validateSavingPath(configuration);
            MatrixLog.i(TAG, "Set saving path: %s", savingPath);
            File file = new File(savingPath);
            file.mkdirs();
            if (!savingPath.endsWith(File.separator)) {
                savingPath += File.separator;
            }
            mWarmUpDelegate.setSavingPath(savingPath);

            // Remove warm up marked file if cool down is set.
            dealWithCoolDown(configuration);

            // Prepare warm-up logic.
            mWarmUpDelegate.prepare(configuration);

            // Set backtrace mode
            boolean hasWarmedUp = WarmUpUtility.hasWarmedUp(configuration.mContext);
            Mode mode = Mode.Quicken;
            if (!hasWarmedUp) {
                if (configuration.mBacktraceMode == Mode.FpUntilQuickenWarmedUp) {
                    mode = Mode.Fp;
                } else if (configuration.mBacktraceMode == Mode.DwarfUntilQuickenWarmedUp) {
                    mode = Mode.Dwarf;
                }
            }
            WeChatBacktraceNative.setBacktraceMode(mode.value);

            MatrixLog.i(TAG, "Has warmed up: %s", hasWarmedUp);

            // Set warmed up flag
            WeChatBacktraceNative.setWarmedUp(hasWarmedUp);

            // Set if invoke generation immediately.
            if (configuration.mImmediateGeneration) {
                WeChatBacktraceNative.immediateGeneration(true);
            }

            // Register warmed up receiver for other processes.
            if (!configuration.mIsWarmUpProcess) {
                mWarmUpDelegate.registerWarmedUpReceiver(configuration, mode);
            }
        } else {
            WeChatBacktraceNative.setBacktraceMode(configuration.mBacktraceMode.value);
        }

        mConfigured = true;

    }

    public static boolean hasWarmedUp(Context context) {
        return WarmUpUtility.hasWarmedUp(context);
    }

    public static int[] _DoStatistic(String pathOfSo) {
        return WeChatBacktraceNative.statistic(pathOfSo);
    }

    public enum Mode {
        Fp(0),
        Quicken(1),
        Dwarf(2),
        FpUntilQuickenWarmedUp(3),
        DwarfUntilQuickenWarmedUp(4);

        int value;

        Mode(int mode) {
            this.value = mode;
        }

        @Override
        public String toString() {
            switch (this) {
                case Fp:
                    return "FramePointer-based.";
                case Quicken:
                    return "WeChat QuickenUnwindTable-based.";
                case Dwarf:
                    return "Dwarf-based.";
                case FpUntilQuickenWarmedUp:
                    return "Use fp-based backtrace before quicken has warmed up.";
                case DwarfUntilQuickenWarmedUp:
                    return "Use dwarf-based backtrace before quicken has warmed up.";
            }
            return "Unreachable.";
        }
    }

    public enum WarmUpTiming {
        WhileScreenOff,
        WhileCharging,
        PostStartup,
    }

    public final static class Configuration {
        Context mContext;
        String mSavingPath;
        HashSet<String> mWarmUpDirectoriesList = new HashSet<>();
        Mode mBacktraceMode = Mode.Quicken;
        LibraryLoader mLibraryLoader = null;
        boolean mCoolDown = false;
        boolean mCoolDownIfApkUpdated = true;   // Default true.
        boolean mIsWarmUpProcess = false;
        boolean mImmediateGeneration = false;
        boolean mWarmUpInIsolateProcess = true;
        WarmUpTiming mWarmUpTiming = WarmUpTiming.WhileScreenOff;
        long mWarmUpDelay = DELAY_SHORTLY;
        boolean mEnableLog = false;
        boolean mEnableIsolateProcessLog = false;
        String mPathOfXLogSo = null;

        private boolean mCommitted = false;
        private WeChatBacktrace mWeChatBacktrace;

        Configuration(Context context, WeChatBacktrace backtrace) {
            mContext = context;
            mWeChatBacktrace = backtrace;

            // Default warm-up
            mWarmUpDirectoriesList.add(context.getApplicationInfo().nativeLibraryDir);
            mWarmUpDirectoriesList.add(WeChatBacktrace.getSystemLibraryPath());
            mWarmUpDirectoriesList.add(WeChatBacktrace.getBaseODEXPath(context));

            mIsWarmUpProcess = ProcessUtil.isMainProcess(mContext);
        }

        public Configuration savingPath(String savingPath) {
            if (mCommitted) {
                return this;
            }
            mSavingPath = savingPath;
            return this;
        }

        public Configuration setBacktraceMode(Mode mode) {
            if (mCommitted) {
                return this;
            }
            if (mode != null) {
                mBacktraceMode = mode;
            }
            return this;
        }

        public Configuration setLibraryLoader(LibraryLoader loader) {
            if (mCommitted) {
                return this;
            }
            mLibraryLoader = loader;
            return this;
        }

        public Configuration immediateGeneration(boolean immedate) {
            if (mCommitted) {
                return this;
            }
            mImmediateGeneration = immedate;
            return this;
        }

        public Configuration directoryToWarmUp(String directory) {
            if (mCommitted) {
                return this;
            }
            mWarmUpDirectoriesList.add(directory);
            return this;
        }

        public Configuration clearWarmUpDirectorySet() {
            if (mCommitted) {
                return this;
            }
            mWarmUpDirectoriesList.clear();
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
            mIsWarmUpProcess = isWarmUpProcess;
            return this;
        }

        public Configuration warmUpInIsolateProcess(boolean isolateProcess) {
            if (mCommitted) {
                return this;
            }
            mWarmUpInIsolateProcess = isolateProcess;
            return this;
        }

        public Configuration warmUpSettings(WarmUpTiming timing, long delayMs) {
            if (mCommitted) {
                return this;
            }

            mWarmUpTiming = timing;
            mWarmUpDelay = delayMs;

            return this;
        }

        public Configuration xLoggerPath(String pathOfXLogSo) {
            if (mCommitted) {
                return this;
            }

            mPathOfXLogSo = pathOfXLogSo;
            return this;
        }

        public Configuration enableOtherProcessLogger(boolean enable) {
            if (mCommitted) {
                return this;
            }

            mEnableLog = enable;

            return this;
        }

        public Configuration enableIsolateProcessLogger(boolean enable) {
            if (mCommitted) {
                return this;
            }

            mEnableIsolateProcessLog = enable;

            return this;
        }

        public void commit() {
            if (mCommitted) {
                return;
            }
            mCommitted = true;

            mWeChatBacktrace.configure(this);
        }

        @Override
        public String toString() {
            return "\n" +
                    "WeChat backtrace configurations: \n" +
                    ">>> Backtrace Mode: " + mBacktraceMode + "\n" +
                    ">>> Saving Path: " + (mSavingPath != null ? mSavingPath : WarmUpUtility.defaultSavingPath(this)) + "\n" +
                    ">>> Custom Library Loader: " + (mLibraryLoader != null) + "\n" +
                    ">>> Directories to Warm-up: " + mWarmUpDirectoriesList.toString() + "\n" +
                    ">>> Is Warm-up Process: " + mIsWarmUpProcess + "\n" +
                    ">>> Warm-up Timing: " + mWarmUpTiming + "\n" +
                    ">>> Warm-up Delay: " + mWarmUpDelay + "ms\n" +
                    ">>> Warm-up in isolate process: " + mWarmUpInIsolateProcess + "\n" +
                    ">>> Invoke quicken generation immediately: " + mImmediateGeneration + "\n" +
                    ">>> Enable logger: " + mEnableLog + "\n" +
                    ">>> Enable Isolate Process logger: " + mEnableIsolateProcessLog + "\n" +
                    ">>> Path of XLog: " + mPathOfXLogSo + "\n" +
                    ">>> Cool-down: " + mCoolDown + "\n" +
                    ">>> Cool-down if Apk Updated: " + mCoolDownIfApkUpdated + "\n"
                    ;
        }

    }
}
