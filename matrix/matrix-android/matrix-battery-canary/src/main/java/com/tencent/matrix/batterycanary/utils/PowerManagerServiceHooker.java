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

package com.tencent.matrix.batterycanary.utils;

import android.content.Context;
import android.os.Build;
import android.os.IBinder;
import android.os.WorkSource;
import android.support.annotation.Nullable;
import android.support.annotation.RestrictTo;

import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

/**
 * This hook is to collect infos about how wake lock is used by the developers.
 *
 * @author liyongjie
 *         Created by liyongjie on 2017/7/24.
 * @Override public void onAcquireWakeLock(IBinder token, int flags, String tag, String packageName, WorkSource workSource, String historyTag) {
 * Log.i(TAG, "onAcquireWakeLock token:" + token.toString() + ", tag:" + tag);
 * }
 * @Override public void onReleaseWakeLock(IBinder token, int flags) {
 * Log.i(TAG, "onReleaseWakeLock token:" + token.toString());
 * }
 * };
 * PowerManagerServiceHooker.addListener(listener);
 * @see IListener
 * <p>
 * usage example:
 * final PowerManagerServiceHooker.IListener listener = new PowerManagerServiceHooker.IListener() {
 */
@RestrictTo(RestrictTo.Scope.LIBRARY)
public final class PowerManagerServiceHooker {
    private static final String TAG = "Matrix.battery.PowerHooker";

    public interface IListener {
        void onAcquireWakeLock(IBinder token, int flags, String tag, String packageName, @Nullable WorkSource workSource, @Nullable String historyTag);
        void onReleaseWakeLock(IBinder token, int flags);
    }

    private static List<IListener> sListeners = new ArrayList<>();
    private static boolean sTryHook;
    private static SystemServiceBinderHooker.HookCallback sHookCallback = new SystemServiceBinderHooker.HookCallback() {
        @Override
        public void onServiceMethodInvoke(Method method, Object[] args) {
            MatrixLog.v(TAG, "onServiceMethodInvoke: method name %s", method.getName());
            dispatchListeners(method, args);
        }

        @Nullable
        @Override
        public Object onServiceMethodIntercept(Object receiver, Method method, Object[] args) {
            return null;
        }
    };
    private static SystemServiceBinderHooker sHookHelper = new SystemServiceBinderHooker(Context.POWER_SERVICE, "android.os.IPowerManager", sHookCallback);

    /**
     * If there is a listener, then hook
     *
     * @param listener
     * @see #removeListener(IListener)
     * @see #checkHook()
     */
    public synchronized static void addListener(IListener listener) {
        if (listener == null) {
            return;
        }

        if (sListeners.contains(listener)) {
            return;
        }

        sListeners.add(listener);
        checkHook();
    }

    /**
     * if there is no listeners, then unHook
     *
     * @param listener
     * @see #checkUnHook()
     */
    public synchronized static void removeListener(IListener listener) {
        if (listener == null) {
            return;
        }

        sListeners.remove(listener);
        checkUnHook();
    }

    public synchronized static void release() {
        sListeners.clear();
        checkUnHook();
    }

    private static void checkHook() {
        if (sTryHook) {
            return;
        }

        if (sListeners.isEmpty()) {
            return;
        }

        boolean hookRet = sHookHelper.doHook();
        MatrixLog.i(TAG, "checkHook hookRet:%b", hookRet);
        sTryHook = true;
    }

    private static void checkUnHook() {
        if (!sTryHook) {
            return;
        }

        if (!sListeners.isEmpty()) {
            return;
        }

        boolean unHookRet = sHookHelper.doUnHook();
        MatrixLog.i(TAG, "checkUnHook unHookRet:%b", unHookRet);
        sTryHook = false;
    }

    private static void dispatchListeners(Method method, Object[] args) {
        if (method.getName().equals("acquireWakeLock")) {
            dispatchAcquireWakeLock(args);
        } else if (method.getName().equals("releaseWakeLock")) {
            dispatchReleaseWakeLock(args);
        }
    }

    /**
     * @see #checkAcquireWakeLockArgs(Object[])
     * @param args
     */
    private static void dispatchAcquireWakeLock(Object[] argsArr) {
        AcquireWakeLockArgs args = AcquireWakeLockArgsCompatible.createAcquireWakeLockArgs(argsArr);
        if (args == null) {
            MatrixLog.w(TAG, "dispatchAcquireWakeLock AcquireWakeLockArgs null");
            return;
        }

        synchronized (PowerManagerServiceHooker.class) {
            for (int i = 0; i < sListeners.size(); i++) {
                sListeners.get(i).onAcquireWakeLock(args.token, args.flags, args.tag,
                        args.packageName, args.ws, args.historyTag);
            }
        }
    }

    private static void dispatchReleaseWakeLock(Object[] argsArr) {
        ReleaseWakeLockArgs args = ReleaseWakeLockArgsCompatible.createReleaseWakeLockArgs(argsArr);
        if (args == null) {
            MatrixLog.w(TAG, "dispatchReleaseWakeLock AcquireWakeLockArgs null");
            return;
        }

        synchronized (PowerManagerServiceHooker.class) {
            for (int i = 0; i < sListeners.size(); i++) {
                sListeners.get(i).onReleaseWakeLock(args.token, args.flags);
            }
        }
    }

    private static final class AcquireWakeLockArgs {
        IBinder token;
        int flags;
        String tag;
        String packageName;
        WorkSource ws;
        String historyTag;
    }

    private static final class AcquireWakeLockArgsCompatible {
        public static AcquireWakeLockArgs createAcquireWakeLockArgs(Object[] argsArr) {
            if (argsArr == null) {
                MatrixLog.w(TAG, "createAcquireWakeLockArgs args null");
                return null;
            }

            //Log infos related
            MatrixLog.i(TAG, "createAcquireWakeLockArgs apiLevel:%d, codeName:%s, versionRelease:%s", Build.VERSION.SDK_INT, Build.VERSION.CODENAME, Build.VERSION.SDK_INT);
            return createAcquireWakeLockArgsAccordingToArgsLength(argsArr);
        }

        private static AcquireWakeLockArgs createAcquireWakeLockArgsAccordingToArgsLength(Object[] argsArr) {
            final int length = argsArr.length;
            MatrixLog.i(TAG, "createAcquireWakeLockArgsAccordingToArgsLength: length:%s", length);
            switch (length) {
                // android I ~ J
                case 4:
                    return createAcquireWakeLockArgs4(argsArr);
                //android K
                case 5:
                    //android L ~ O
                case 6:
                default:
                    return createAcquireWakeLockArgs6or5(argsArr);
            }
        }

        private static AcquireWakeLockArgs createAcquireWakeLockArgs6or5(Object[] argsArr) {
            if (argsArr.length != 6 && argsArr.length != 5) {
                MatrixLog.w(TAG, "createAcquireWakeLockArgs6 args length invalid : %d", argsArr.length);
                return null;
            }

            AcquireWakeLockArgs args = new AcquireWakeLockArgs();

            if (!(argsArr[0] instanceof IBinder)) {
                MatrixLog.w(TAG, "createAcquireWakeLockArgs6 args idx 0 not IBinder, %s", argsArr[0]);
                return null;
            } else {
                args.token = (IBinder) argsArr[0];
            }

            if (!(argsArr[1] instanceof Integer)) {
                MatrixLog.w(TAG, "createAcquireWakeLockArgs6 args idx 1 not Integer, %s", argsArr[1]);
                return null;
            } else {
                args.flags = (Integer) argsArr[1];
            }

            if (argsArr[2] != null && !(argsArr[2] instanceof String)) {
                MatrixLog.w(TAG, "createAcquireWakeLockArgs6 args idx 2 not String, %s", argsArr[2]);
                return null;
            } else {
                args.tag = (String) argsArr[2];
            }

            if (argsArr[3] != null && !(argsArr[3] instanceof String)) {
                MatrixLog.w(TAG, "createAcquireWakeLockArgs6 args idx 3 not String, %s", argsArr[3]);
                return null;
            } else {
                args.packageName = (String) argsArr[3];
            }

            if (argsArr[4] != null && !(argsArr[4] instanceof WorkSource)) {
                MatrixLog.w(TAG, "createAcquireWakeLockArgs6 args idx 4 not WorkSource, %s", argsArr[4]);
                return null;
            } else {
                args.ws = (WorkSource) argsArr[4];
            }

            if (argsArr.length == 5) {
                return args;
            }

            if (argsArr[5] != null && !(argsArr[5] instanceof String)) {
                MatrixLog.w(TAG, "createAcquireWakeLockArgs6 args idx 5 not String, %s", argsArr[5]);
                return null;
            } else {
                args.historyTag = (String) argsArr[5];
            }

            return args;
        }

        private static AcquireWakeLockArgs createAcquireWakeLockArgs4(Object[] argsArr) {
            if (argsArr.length != 4) {
                MatrixLog.w(TAG, "createAcquireWakeLockArgs4 args length invalid : %d", argsArr.length);
                return null;
            }

            AcquireWakeLockArgs args = new AcquireWakeLockArgs();
            if (argsArr[2] != null && !(argsArr[2] instanceof String)) {
                MatrixLog.w(TAG, "createAcquireWakeLockArgs6 args idx 2 not String, %s", argsArr[2]);
                return null;
            } else {
                args.tag = (String) argsArr[2];
            }

            if (argsArr[3] != null && !(argsArr[3] instanceof WorkSource)) {
                MatrixLog.w(TAG, "createAcquireWakeLockArgs6 args idx 3 not WorkSource, %s", argsArr[3]);
                return null;
            } else {
                args.ws = (WorkSource) argsArr[3];
            }

            // API 15 ~ 16
            if (argsArr[0] instanceof Integer) {
                args.flags = (Integer) argsArr[0];
                if (!(argsArr[1] instanceof IBinder)) {
                    MatrixLog.w(TAG, "createAcquireWakeLockArgs6 args idx 1 not IBinder, %s", argsArr[1]);
                    return null;
                } else {
                    args.token = (IBinder) argsArr[1];
                }
                // API 17 ~ 18
            } else if (argsArr[0] instanceof IBinder) {
                args.token = (IBinder) argsArr[0];
                if (!(argsArr[1] instanceof Integer)) {
                    MatrixLog.w(TAG, "createAcquireWakeLockArgs4 args idx 1 not Integer, %s", argsArr[1]);
                    return null;
                } else {
                    args.flags = (Integer) argsArr[1];
                }
            } else {
                MatrixLog.w(TAG, "createAcquireWakeLockArgs4 args idx 0 not IBinder an Integer, %s", argsArr[0]);
                return null;
            }

            return args;
        }
    }

    private static final class ReleaseWakeLockArgs {
        IBinder token;
        int flags;
    }

    private static final class ReleaseWakeLockArgsCompatible {
        public static ReleaseWakeLockArgs createReleaseWakeLockArgs(Object[] argsArr) {
            if (argsArr == null) {
                MatrixLog.w(TAG, "createReleaseWakeLockArgs args null");
                return null;
            }

            //Log infos related
            MatrixLog.i(TAG, "createReleaseWakeLockArgs apiLevel:%d, codeName:%s, versionRelease:%s", Build.VERSION.SDK_INT, Build.VERSION.CODENAME, Build.VERSION.SDK_INT);
            return createReleaseWakeLockArgsAccordingToArgsLength(argsArr);
        }

        private static ReleaseWakeLockArgs createReleaseWakeLockArgsAccordingToArgsLength(Object[] argsArr) {
            final int length = argsArr.length;
            MatrixLog.i(TAG, "createReleaseWakeLockArgsAccordingToArgsLength: length:%s", length);
            switch (length) {
                case 2:
                default:
                    return createReleaseWakeLockArgs2(argsArr);
            }
        }

        private static ReleaseWakeLockArgs createReleaseWakeLockArgs2(Object[] argsArr) {
            if (argsArr.length != 2) {
                MatrixLog.w(TAG, "createReleaseWakeLockArgs2 args length invalid : %d", argsArr.length);
                return null;
            }

            ReleaseWakeLockArgs args = new ReleaseWakeLockArgs();

            if (!(argsArr[0] instanceof IBinder)) {
                MatrixLog.w(TAG, "createReleaseWakeLockArgs2 args idx 0 not IBinder, %s", argsArr[0]);
                return null;
            } else {
                args.token = (IBinder) argsArr[0];
            }

            if (!(argsArr[1] instanceof Integer)) {
                MatrixLog.w(TAG, "createReleaseWakeLockArgs2 args idx 1 not Integer, %s", argsArr[1]);
                return null;
            } else {
                args.flags = (Integer) argsArr[1];
            }
            return args;
        }
    }
}
