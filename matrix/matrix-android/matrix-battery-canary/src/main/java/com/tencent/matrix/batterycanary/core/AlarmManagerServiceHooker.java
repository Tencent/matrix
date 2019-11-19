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

package com.tencent.matrix.batterycanary.core;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.os.Build;

import com.tencent.matrix.batterycanary.util.SystemServiceBinderHooker;
import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

/**
 * @author liyongjie
 *         Created by liyongjie on 2017/10/30.
 */

public class AlarmManagerServiceHooker {
    private static final String TAG = "Matrix.AlarmManagerServiceHooker";

    public interface IListener {
        void onAlarmSet(int type, long triggerAtMillis, long windowMillis, long intervalMillis,
                        int flags, PendingIntent operation, AlarmManager.OnAlarmListener onAlarmListener);
        void onAlarmRemove(PendingIntent operation, AlarmManager.OnAlarmListener onAlarmListener);
    }

    private static boolean sTryHook;
    private static SystemServiceBinderHooker.HookCallback sHookCallback = new SystemServiceBinderHooker.HookCallback() {
        @Override
        public void onServiceMethodInvoke(Method method, Object[] args) {
            MatrixLog.v(TAG, "onServiceMethodInvoke: method name %s", method.getName());
            dispatchListeners(method, args);
        }
    };
    private static SystemServiceBinderHooker sHookHelper = new SystemServiceBinderHooker(Context.ALARM_SERVICE, "android.app.IAlarmManager", sHookCallback);

    private static List<IListener> sListeners = new ArrayList<>();

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
        if (method.getName().equals("set")
                //jb-release ics-mr0-release
                || method.getName().equals("setRepeating") || method.getName().equals("setInexactRepeating")) {
            dispatchSet(args);
        } else if (method.getName().equals("remove")) {
            dispatchCancel(args);
        }
    }

    private static void dispatchSet(Object[] args) {
        SetArgs setArgs = SetArgsCompatible.createSetArgs(args);
        if (setArgs == null) {
            MatrixLog.w(TAG, "dispatchSet setArgs null");
            return;
        }

        synchronized (AlarmManagerServiceHooker.class) {
            for (int i = 0; i < sListeners.size(); i++) {
                sListeners.get(i).onAlarmSet(setArgs.type, setArgs.triggerAtMillis, setArgs.windowMillis,
                        setArgs.intervalMillis, setArgs.flags, setArgs.operation, setArgs.onAlarmListener);
            }
        }
    }

    private static void dispatchCancel(Object[] args) {
        CancelArgs cancelArgs = CancelArgsCompatible.createCancelArgs(args);
        if (cancelArgs == null) {
            MatrixLog.w(TAG, "dispatchCancel cancelArgs null");
            return;
        }

        synchronized (AlarmManagerServiceHooker.class) {
            for (int i = 0; i < sListeners.size(); i++) {
                sListeners.get(i).onAlarmRemove(cancelArgs.operation, cancelArgs.onAlarmListener);
            }
        }
    }

    private static Class sListenerWrapperCls;
    private static Field sListenerField;
    static {
        try {
            sListenerWrapperCls = Class.forName("android.app.AlarmManager$ListenerWrapper");
            sListenerField = sListenerWrapperCls.getDeclaredField("mListener");
            sListenerField.setAccessible(true);
        } catch (ClassNotFoundException e) {
            MatrixLog.w(TAG, "static init exp:%s", e);
        } catch (NoSuchFieldException e) {
            MatrixLog.w(TAG, "static init exp:%s", e);
        }
    }

    private static final class SetArgs {
        int type;
        long triggerAtMillis;
        long windowMillis;
        long intervalMillis;
        int flags;
        PendingIntent operation;
        AlarmManager.OnAlarmListener onAlarmListener;
    }

    private static final class SetArgsCompatible {

        public static SetArgs createSetArgs(Object[] argsArr) {
            if (argsArr == null) {
                MatrixLog.w(TAG, "createSetArgs args null");
                return null;
            }

            //Log infos related
            MatrixLog.i(TAG, "createSetArgs apiLevel:%d, codeName:%s, versionRelease:%s", Build.VERSION.SDK_INT, Build.VERSION.CODENAME, Build.VERSION.SDK_INT);

            return createSetArgsAccordingToArgsLength(argsArr);
        }

        private static SetArgs createSetArgsAccordingToArgsLength(Object[] argsArr) {
            final int length = argsArr.length;
            MatrixLog.i(TAG, "createSetArgsAccordingToArgsLength: length:%s", length);
            switch (length) {
                case 3:
                    //jb-release ics-mr0-release set
                    return createSetArgs3(argsArr);
                case 4:
                    //jb-release ics-mr0-release setRepeating setInexactRepeating
                    return createSetArgs4(argsArr);
                //kitkat-release
                case 6:
                //lollipop-release
                case 7:
                    return createSetArgs7or6(argsArr);
                //marshmallow-release
                case 8:
                    return createSetArgs8(argsArr);
                //nougat-release and oreo-release
                case 11:
                default:
                    return createSetArgs11(argsArr);
            }
        }

        private static SetArgs createSetArgs11(Object[] argsArr) {
            if (argsArr.length != 11) {
                MatrixLog.w(TAG, "createSetArgs args length invalid : %d", argsArr.length);
                return null;
            }

            SetArgs setArgs = new SetArgs();
            if (!(argsArr[1] instanceof Integer)) {
                MatrixLog.w(TAG, "createSetArgs args idx 1 not Integer, %s", argsArr[1]);
                return null;
            } else {
                setArgs.type = (Integer) argsArr[1];
            }

            if (!(argsArr[2] instanceof Long)) {
                MatrixLog.w(TAG, "createSetArgs args idx 2 not Long, %s", argsArr[2]);
                return null;
            } else {
                setArgs.triggerAtMillis = (Long) argsArr[2];
            }

            if (!(argsArr[3] instanceof Long)) {
                MatrixLog.w(TAG, "createSetArgs args idx 3 not Long, %s", argsArr[3]);
                return null;
            } else {
                setArgs.windowMillis = (Long) argsArr[3];
            }

            if (!(argsArr[4] instanceof Long)) {
                MatrixLog.w(TAG, "createSetArgs args idx 4 not Long, %s", argsArr[4]);
                return null;
            } else {
                setArgs.intervalMillis = (Long) argsArr[4];
            }

            if (!(argsArr[5] instanceof Integer)) {
                MatrixLog.w(TAG, "createSetArgs args idx 5 not Integer, %s", argsArr[5]);
                return null;
            } else {
                setArgs.flags = (Integer) argsArr[5];
            }

            if (argsArr[6] != null && !(argsArr[6] instanceof PendingIntent)) {
                MatrixLog.w(TAG, "createSetArgs args idx 6 not PendingIntent, %s", argsArr[6]);
                return null;
            } else {
                setArgs.operation = (PendingIntent) argsArr[6];
            }

            if (sListenerWrapperCls == null || sListenerField == null) {
                MatrixLog.w(TAG, "createSetArgs sListenerWrapperCls sListenerField null");
                return null;
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                if (argsArr[7] != null && !sListenerWrapperCls.isInstance(argsArr[7])) {
                    MatrixLog.w(TAG, "createSetArgs args idx 7 not ListenerWrapper, %s", argsArr[7]);
                    return null;
                } else {
                    try {
                        if (argsArr[7] != null) {
                            setArgs.onAlarmListener = (AlarmManager.OnAlarmListener) sListenerField.get(argsArr[7]);
                        }
                    } catch (IllegalAccessException e) {
                        MatrixLog.w(TAG, "createSetArgs args idx 7 init exp:%s", e.getLocalizedMessage());
                        return null;
                    }
                }
            }

            return setArgs;
        }

        private static SetArgs createSetArgs8(Object[] argsArr) {
            if (argsArr.length != 8) {
                MatrixLog.w(TAG, "createSetArgs args length invalid : %d", argsArr.length);
                return null;
            }

            SetArgs setArgs = new SetArgs();
            if (!(argsArr[0] instanceof Integer)) {
                MatrixLog.w(TAG, "createSetArgs args idx 0 not Integer, %s", argsArr[0]);
                return null;
            } else {
                setArgs.type = (Integer) argsArr[0];
            }

            if (!(argsArr[1] instanceof Long)) {
                MatrixLog.w(TAG, "createSetArgs args idx 1 not Long, %s", argsArr[1]);
                return null;
            } else {
                setArgs.triggerAtMillis = (Long) argsArr[1];
            }

            if (!(argsArr[2] instanceof Long)) {
                MatrixLog.w(TAG, "createSetArgs args idx 2 not Long, %s", argsArr[2]);
                return null;
            } else {
                setArgs.windowMillis = (Long) argsArr[2];
            }

            if (!(argsArr[3] instanceof Long)) {
                MatrixLog.w(TAG, "createSetArgs args idx 3 not Long, %s", argsArr[3]);
                return null;
            } else {
                setArgs.intervalMillis = (Long) argsArr[3];
            }

            if (!(argsArr[4] instanceof Integer)) {
                MatrixLog.w(TAG, "createSetArgs args idx 4 not Integer, %s", argsArr[4]);
                return null;
            } else {
                setArgs.flags = (Integer) argsArr[4];
            }

            if (argsArr[5] != null && !(argsArr[5] instanceof PendingIntent)) {
                MatrixLog.w(TAG, "createSetArgs args idx 5 not PendingIntent, %s", argsArr[5]);
                return null;
            } else {
                setArgs.operation = (PendingIntent) argsArr[5];
            }
            return setArgs;
        }

        private static SetArgs createSetArgs7or6(Object[] argsArr) {
            if (argsArr.length != 7 && argsArr.length != 6) {
                MatrixLog.w(TAG, "createSetArgs args length invalid : %d", argsArr.length);
                return null;
            }

            SetArgs setArgs = new SetArgs();
            if (!(argsArr[0] instanceof Integer)) {
                MatrixLog.w(TAG, "createSetArgs args idx 0 not Integer, %s", argsArr[0]);
                return null;
            } else {
                setArgs.type = (Integer) argsArr[0];
            }

            if (!(argsArr[1] instanceof Long)) {
                MatrixLog.w(TAG, "createSetArgs args idx 1 not Long, %s", argsArr[1]);
                return null;
            } else {
                setArgs.triggerAtMillis = (Long) argsArr[1];
            }

            if (!(argsArr[2] instanceof Long)) {
                MatrixLog.w(TAG, "createSetArgs args idx 2 not Long, %s", argsArr[2]);
                return null;
            } else {
                setArgs.windowMillis = (Long) argsArr[2];
            }

            if (!(argsArr[3] instanceof Long)) {
                MatrixLog.w(TAG, "createSetArgs args idx 3 not Long, %s", argsArr[3]);
                return null;
            } else {
                setArgs.intervalMillis = (Long) argsArr[3];
            }

            if (argsArr[4] != null && !(argsArr[4] instanceof PendingIntent)) {
                MatrixLog.w(TAG, "createSetArgs args idx 4 not PendingIntent, %s", argsArr[4]);
                return null;
            } else {
                setArgs.operation = (PendingIntent) argsArr[4];
            }
            return setArgs;
        }

        private static SetArgs createSetArgs4(Object[] argsArr) {
            if (argsArr.length != 4) {
                MatrixLog.w(TAG, "createSetArgs args length invalid : %d", argsArr.length);
                return null;
            }

            SetArgs setArgs = new SetArgs();
            if (!(argsArr[0] instanceof Integer)) {
                MatrixLog.w(TAG, "createSetArgs args idx 0 not Integer, %s", argsArr[0]);
                return null;
            } else {
                setArgs.type = (Integer) argsArr[0];
            }

            if (!(argsArr[1] instanceof Long)) {
                MatrixLog.w(TAG, "createSetArgs args idx 1 not Long, %s", argsArr[1]);
                return null;
            } else {
                setArgs.triggerAtMillis = (Long) argsArr[1];
            }

            if (!(argsArr[2] instanceof Long)) {
                MatrixLog.w(TAG, "createSetArgs args idx 2 not Long, %s", argsArr[2]);
                return null;
            } else {
                setArgs.intervalMillis = (Long) argsArr[2];
            }

            if (argsArr[3] != null && !(argsArr[3] instanceof PendingIntent)) {
                MatrixLog.w(TAG, "createSetArgs args idx 3 not PendingIntent, %s", argsArr[3]);
                return null;
            } else {
                setArgs.operation = (PendingIntent) argsArr[3];
            }
            return setArgs;
        }

        private static SetArgs createSetArgs3(Object[] argsArr) {
            if (argsArr.length != 3) {
                MatrixLog.w(TAG, "createSetArgs args length invalid : %d", argsArr.length);
                return null;
            }

            SetArgs setArgs = new SetArgs();
            if (!(argsArr[0] instanceof Integer)) {
                MatrixLog.w(TAG, "createSetArgs args idx 0 not Integer, %s", argsArr[0]);
                return null;
            } else {
                setArgs.type = (Integer) argsArr[0];
            }

            if (!(argsArr[1] instanceof Long)) {
                MatrixLog.w(TAG, "createSetArgs args idx 1 not Long, %s", argsArr[1]);
                return null;
            } else {
                setArgs.triggerAtMillis = (Long) argsArr[1];
            }

            if (argsArr[2] != null && !(argsArr[2] instanceof PendingIntent)) {
                MatrixLog.w(TAG, "createSetArgs args idx 2 not PendingIntent, %s", argsArr[2]);
                return null;
            } else {
                setArgs.operation = (PendingIntent) argsArr[2];
            }
            return setArgs;
        }
    }

    private static final class CancelArgs {
        PendingIntent operation;
        AlarmManager.OnAlarmListener onAlarmListener;
    }

    private static final class CancelArgsCompatible {
        public static CancelArgs createCancelArgs(Object[] argsArr) {
            if (argsArr == null) {
                MatrixLog.w(TAG, "createCancelArgs args null");
                return null;
            }

            //Log infos related
            MatrixLog.i(TAG, "createCancelArgs apiLevel:%d, codeName:%s, versionRelease:%s", Build.VERSION.SDK_INT, Build.VERSION.CODENAME, Build.VERSION.SDK_INT);

            return createCancelArgsAccordingToArgsLength(argsArr);
        }

        private static CancelArgs createCancelArgsAccordingToArgsLength(Object[] argsArr) {
            final int length = argsArr.length;
            MatrixLog.i(TAG, "createCancelArgsAccordingToArgsLength: length:%s", length);
            switch (length) {
                //i to m
                case 1:
                    return createCancelArgs1(argsArr);
                //oreo-release nougat-release
                case 2:
                default:
                    return createCancelArgs2(argsArr);
            }
        }

        private static CancelArgs createCancelArgs2(Object[] argsArr) {
            if (argsArr.length != 2) {
                MatrixLog.w(TAG, "createCancelArgs2 args length invalid : %d", argsArr.length);
                return null;
            }

            CancelArgs cancelArgs = new CancelArgs();
            if (argsArr[0] != null && !(argsArr[0] instanceof PendingIntent)) {
                MatrixLog.w(TAG, "createCancelArgs2 args idx 0 not PendingIntent, %s", argsArr[0]);
                return null;
            } else {
                cancelArgs.operation = (PendingIntent) argsArr[0];
            }

            if (sListenerWrapperCls == null || sListenerField == null) {
                MatrixLog.w(TAG, "createSetArgs sListenerWrapperCls sListenerField null");
                return null;
            }
            if (argsArr[1] != null && !sListenerWrapperCls.isInstance(argsArr[1])) {
                MatrixLog.w(TAG, "createSetArgs args idx 1 not ListenerWrapper, %s", argsArr[1]);
                return null;
            } else {
                try {
                    if (argsArr[1] != null) {
                        cancelArgs.onAlarmListener = (AlarmManager.OnAlarmListener) sListenerField.get(argsArr[1]);
                    }
                } catch (IllegalAccessException e) {
                    MatrixLog.w(TAG, "createSetArgs args idx 1 init exp:%s", e.getLocalizedMessage());
                    return null;
                }
            }

            return cancelArgs;
        }

        private static CancelArgs createCancelArgs1(Object[] argsArr) {
            if (argsArr.length != 1) {
                MatrixLog.w(TAG, "createCancelArgs1 args length invalid : %d", argsArr.length);
                return null;
            }

            CancelArgs cancelArgs = new CancelArgs();
            if (argsArr[0] != null && !(argsArr[0] instanceof PendingIntent)) {
                MatrixLog.w(TAG, "createCancelArgs1 args idx 0 not PendingIntent, %s", argsArr[0]);
                return null;
            } else {
                cancelArgs.operation = (PendingIntent) argsArr[0];
            }
            return cancelArgs;
        }
    }
}
