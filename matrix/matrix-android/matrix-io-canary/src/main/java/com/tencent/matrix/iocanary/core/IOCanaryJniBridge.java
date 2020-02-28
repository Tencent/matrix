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

package com.tencent.matrix.iocanary.core;

import com.tencent.matrix.iocanary.config.IOConfig;
import com.tencent.matrix.iocanary.util.IOCanaryUtil;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;

/**
 * Created by liyongjie on 2017/12/8.
 */

public class IOCanaryJniBridge {
    private static final String TAG = "Matrix.IOCanaryJniBridge";

    private static OnJniIssuePublishListener sOnIssuePublishListener;
    private static boolean sIsTryInstall;
    private static boolean sIsLoadJniLib;

    public static void install(IOConfig config, OnJniIssuePublishListener listener) {
        MatrixLog.v(TAG, "install sIsTryInstall:%b", sIsTryInstall);
        if (sIsTryInstall) {
            return;
        }

        //load lib
        if (!loadJni()) {
            MatrixLog.e(TAG, "install loadJni failed");
            return;
        }

        //set listener
        sOnIssuePublishListener = listener;

        try {
            //set config
            if (config != null) {
                if (config.isDetectFileIOInMainThread()) {
                    enableDetector(DetectorType.MAIN_THREAD_IO);
                    // ms to μs
                    setConfig(ConfigKey.MAIN_THREAD_THRESHOLD, config.getFileMainThreadTriggerThreshold() * 1000L);
                }

                if (config.isDetectFileIOBufferTooSmall()) {
                    enableDetector(DetectorType.SMALL_BUFFER);
                    setConfig(ConfigKey.SMALL_BUFFER_THRESHOLD, config.getFileBufferSmallThreshold());
                }

                if (config.isDetectFileIORepeatReadSameFile()) {
                    enableDetector(DetectorType.REPEAT_READ);
                    setConfig(ConfigKey.REPEAT_READ_THRESHOLD, config.getFileRepeatReadThreshold());
                }
            }

            //hook
            doHook();

            sIsTryInstall = true;
        } catch (Error e) {
            MatrixLog.printErrStackTrace(TAG, e, "call jni method error");
        }
    }

    public static void uninstall() {
        if (!sIsTryInstall) {
            return;
        }

        doUnHook();
        sIsTryInstall = false;
    }

    private static boolean loadJni() {
        if (sIsLoadJniLib) {
            return true;
        }

        try {
            System.loadLibrary("io-canary");
        } catch (Exception e) {
            MatrixLog.e(TAG, "hook: e: %s", e.getLocalizedMessage());
            sIsLoadJniLib = false;
            return false;
        }

        sIsLoadJniLib = true;
        return true;
    }

    public static void onIssuePublish(ArrayList<IOIssue> issues) {
        if (sOnIssuePublishListener == null) {
            return;
        }

        sOnIssuePublishListener.onIssuePublish(issues);
    }

    private static final class JavaContext {
        private final String stack;
        private String threadName;

        private JavaContext() {
            stack = IOCanaryUtil.getThrowableStack(new Throwable());
            if (null != Thread.currentThread()) {
                threadName = Thread.currentThread().getName();
            }
        }
    }

    /**
     * 声明为private，给c++部分调用！！！不要干掉！！！
     * @return
     */
    private static JavaContext getJavaContext() {
        try {
            return new JavaContext();
        } catch (Throwable th) {
            MatrixLog.printErrStackTrace(TAG, th, "get javacontext exception");
        }

        return null;
    }

    /**
     *  enum DetectorType {
     *    kDetectorMainThreadIO = 0,
     *    kDetectorSmallBuffer,
     *    kDetectorRepeatRead
     *  };
     */
    private static final class DetectorType {
        static final int MAIN_THREAD_IO = 0;
        static final int SMALL_BUFFER = 1;
        static final int REPEAT_READ = 2;
    }
    private static native void enableDetector(int detectorType);

    /**
     * enum IOCanaryConfigKey {
     *    kMainThreadThreshold = 0,
     *    kSmallBufferThreshold,
     *    kRepeatReadThreshold,
     * };
     */
    private static final class ConfigKey {
        static final int MAIN_THREAD_THRESHOLD = 0;
        static final int SMALL_BUFFER_THRESHOLD = 1;
        static final int REPEAT_READ_THRESHOLD = 2;
    }

    private static native void setConfig(int key, long val);

    private static native boolean doHook();

    private static native boolean doUnHook();
}
