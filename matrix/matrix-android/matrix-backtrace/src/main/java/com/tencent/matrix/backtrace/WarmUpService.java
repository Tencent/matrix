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

import android.annotation.SuppressLint;
import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;

import androidx.annotation.Nullable;

import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.xlog.XLogNative;

import java.util.concurrent.atomic.AtomicInteger;

public class WarmUpService extends Service {

    private final static String TAG = "Matrix.WarmUpService";

    interface RemoteInvoker {
        Bundle call(int cmd, Bundle args);
    }

    interface RemoteConnection {
        boolean isConnected();
        boolean connect(Context context, Bundle args);
        void disconnect(Context context);
    }

    private final static String INVOKE_ARGS = "invoke-args";
    private final static String INVOKE_RESP = "invoke-resp";

    final static class RemoteInvokerImpl implements RemoteInvoker, RemoteConnection {

        private final static String TAG = "Matrix.WarmUpInvoker";

        volatile Messenger mResp;
        volatile Messenger mReq;

        final Bundle[] mResult = {null};
        final HandlerThread[] mHandlerThread = {null};

        ServiceConnection mConnection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                mReq = new Messenger(service);
                synchronized (mBound) {
                    mBound[0] = true;
                    mBound.notifyAll();
                }
                MatrixLog.i(TAG, "This remote invoker(%s) connected.", this);
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {
                mReq = null;
                synchronized (mBound) {
                    mBound[0] = false;
                    mBound.notifyAll();
                }
                MatrixLog.i(TAG, "This remote invoker(%s) disconnected.", this);

                synchronized (mResult) {
                    mResult[0] = null;
                    mResult.notifyAll();
                }
            }
        };

        private final boolean[] mBound = {false};

        private void checkThread() {
            if (Looper.getMainLooper() == Looper.myLooper()) {
                throw new RuntimeException("Should not call this from main thread!");
            }
        }

        @Override
        public boolean isConnected() {
            return mBound[0];
        }

        @Override
        public boolean connect(Context context, Bundle args) {

            checkThread();

            if (mBound[0]) {
                return true;
            }

            MatrixLog.i(TAG, "Start connecting to remote. (%s)", this.hashCode());

            synchronized (mHandlerThread) {

                if (mHandlerThread[0] != null) {
                    mHandlerThread[0].quitSafely();
                    mHandlerThread[0] = null;
                }

                mHandlerThread[0] = new HandlerThread("warm-up-remote-invoker-"
                        + this.hashCode());
                mHandlerThread[0].start();

                mResp = new Messenger(new Handler(mHandlerThread[0].getLooper()) {
                    public void handleMessage(Message msg) {
                        if (msg.obj instanceof Bundle) {
                            Bundle bundle = (Bundle) msg.obj;
                            synchronized (mResult) {
                                mResult[0] = bundle;
                                mResult.notifyAll();
                            }
                        }
                    }
                });
            }

            Intent intent = new Intent();
            intent.setComponent(new ComponentName(context, WarmUpService.class));
            intent.putExtra(BIND_ARGS_ENABLE_LOGGER, args.getBoolean(BIND_ARGS_ENABLE_LOGGER, false));
            intent.putExtra(BIND_ARGS_PATH_OF_XLOG_SO, args.getString(BIND_ARGS_PATH_OF_XLOG_SO, null));
            context.bindService(intent, mConnection, BIND_AUTO_CREATE);

            try {
                synchronized (mBound) {
                    if (!mBound[0]) {
                        mBound.wait(60000); // 60s
                    }
                }
            } catch (InterruptedException e) {
                MatrixLog.printErrStackTrace(TAG, e, "");
            }

            if (!mBound[0]) {
                disconnect(context);
            }

            return mBound[0];

        }

        @Override
        public void disconnect(Context context) {
            try {
                context.unbindService(mConnection);
            } catch (Throwable e) {
                MatrixLog.printErrStackTrace(TAG, e, "");
            }

            MatrixLog.i(TAG, "Start disconnecting to remote. (%s)", this.hashCode());

            synchronized (mHandlerThread) {
                if (mHandlerThread[0] != null) {
                    mHandlerThread[0].quitSafely();
                    mHandlerThread[0] = null;
                }
            }

            synchronized (mResult) {
                mResult[0] = null;
                mResult.notifyAll();
            }
        }

        @Override
        public Bundle call(int cmd, Bundle args) {
            try {
                Messenger req = mReq;
                if (req != null) {
                    Bundle wrap = new Bundle();
                    wrap.putBundle(INVOKE_ARGS, args);
                    wrap.putBinder(INVOKE_RESP, mResp.getBinder());
                    req.send(Message.obtain(null, cmd, wrap));

                    synchronized (mResult) {
                        mResult[0] = null;
                        mResult.wait(300 * 1000); // 300s
                        return mResult[0];
                    }
                }
            } catch (RemoteException | InterruptedException e) {
                MatrixLog.printErrStackTrace(TAG, e, "");
            }

            return null;
        }
    }

    final static String BIND_ARGS_PATH_OF_XLOG_SO = "path-of-xlog-so";
    final static String BIND_ARGS_ENABLE_LOGGER = "enable-logger";

    final static String ARGS_WARM_UP_SAVING_PATH = "saving-path";
    final static String ARGS_WARM_UP_PATH_OF_ELF = "path-of-elf";
    final static String ARGS_WARM_UP_ELF_START_OFFSET = "elf-start-offset";
    final static String RESULT_OF_WARM_UP = "warm-up-result";

    final static int CMD_WARM_UP_SINGLE_ELF_FILE = 100;

    private final Messenger mMessenger = new Messenger(new Handler() {
        @SuppressLint("HandlerLeak")
        @Override
        public void handleMessage(final Message msg) {
            super.handleMessage(msg);

            if (msg.obj instanceof Bundle) {
                Bundle wrap = (Bundle) msg.obj;
                Bundle args = wrap.getBundle(INVOKE_ARGS);
                IBinder binder = wrap.getBinder(INVOKE_RESP);

                Bundle result = call(msg.what, args);

                Messenger messenger = new Messenger(binder);

                try {
                    messenger.send(Message.obtain(null, msg.what, result));
                } catch (RemoteException e) {
                    MatrixLog.printErrStackTrace(TAG, e, "");
                }
            }
        }
    });


    final static int OK = 0;
    public static final int INVALID_ARGUMENT = -1;
    public static final int WARM_UP_FAILED = -2;
    public static final int WARM_UP_FAILED_TOO_MANY_TIMES = -3;

    private static volatile boolean sHasInitiated = false;
    private static volatile boolean sHasLoaded = false;
    private static HandlerThread sRecycler;
    private static Handler sRecyclerHandler;
    private static final AtomicInteger sWorkingCall = new AtomicInteger(0);
    private final static byte[] sRecyclerLock = new byte[0];
    private final WarmUpDelegate mWarmUpDelegate = new WarmUpDelegate();
    private final static int MSG_SUICIDE = 1;
    private final static long INTERVAL_OF_CHECK = 60 * 1000; // ms

    private final static class RecyclerCallback implements Handler.Callback {

        @Override
        public boolean handleMessage(Message msg) {
            if (msg.what == MSG_SUICIDE) {
                MatrixLog.i(TAG, "Suicide.");
                android.os.Process.killProcess(android.os.Process.myPid());
                System.exit(0);
            }
            return false;
        }
    }

    private synchronized static void loadLibrary(Intent intent) {

        if (sHasLoaded) {
            return;
        }

        MatrixLog.i(TAG, "Init called.");
        WeChatBacktrace.loadLibrary();

        boolean enableLogger = intent.getBooleanExtra(BIND_ARGS_ENABLE_LOGGER, false);
        String pathOfXLogSo = intent.getStringExtra(BIND_ARGS_PATH_OF_XLOG_SO);

        MatrixLog.i(TAG, "Enable logger: %s", enableLogger);
        MatrixLog.i(TAG, "Path of XLog: %s", pathOfXLogSo);

        XLogNative.setXLogger(pathOfXLogSo);
        WeChatBacktrace.enableLogger(enableLogger);

        sHasLoaded = true;
    }

    private synchronized static void init() {

        if (sHasInitiated) {
            return;
        }

        synchronized (sRecyclerLock) {
            if (sRecycler == null) {
                sRecycler = new HandlerThread("backtrace-recycler");
                sRecycler.start();
                sRecyclerHandler = new Handler(sRecycler.getLooper(), new RecyclerCallback());
            }
        }

        scheduleSuicide(true);

        sHasInitiated = true;
    }

    private boolean isNullOrNil(String string) {
        return string == null || string.isEmpty();
    }

    private static void scheduleSuicide(boolean onCreate) {
        MatrixLog.i(TAG, "Schedule suicide");
        synchronized (sRecyclerLock) {
            if (onCreate) {
                sRecyclerHandler.sendEmptyMessageDelayed(MSG_SUICIDE, INTERVAL_OF_CHECK);
            } else {
                if (sWorkingCall.decrementAndGet() == 0) {
                    sRecyclerHandler.sendEmptyMessageDelayed(MSG_SUICIDE, INTERVAL_OF_CHECK);
                }
            }
        }
    }

    private void removeScheduledSuicide() {
        MatrixLog.i(TAG, "Remove scheduled suicide");
        synchronized (sRecyclerLock) {
            sRecyclerHandler.removeMessages(MSG_SUICIDE);
            sWorkingCall.getAndIncrement();
        }
    }

    private synchronized Bundle call(int cmd, Bundle args) {

        removeScheduledSuicide();
        try {
            final Bundle result = new Bundle();
            result.putInt(RESULT_OF_WARM_UP, INVALID_ARGUMENT);
            if (args == null) {
                MatrixLog.i(TAG, "Args is null.");
                return result;
            }
            String savingPath = args.getString(ARGS_WARM_UP_SAVING_PATH, null);
            MatrixLog.i(TAG, "Invoke from client with savingPath: %s.", savingPath);
            if (isNullOrNil(savingPath)) {
                MatrixLog.i(TAG, "Saving path is empty.");
                return result;
            }
            mWarmUpDelegate.setSavingPath(savingPath);

            if (cmd == CMD_WARM_UP_SINGLE_ELF_FILE) {

                String pathOfSo = args.getString(ARGS_WARM_UP_PATH_OF_ELF, null);
                if (isNullOrNil(pathOfSo)) {
                    MatrixLog.i(TAG, "Warm-up so path is empty.");
                    return result;
                }
                int offset = args.getInt(ARGS_WARM_UP_ELF_START_OFFSET, 0);

                MatrixLog.i(TAG, "Warm up so path %s offset %s.", pathOfSo, offset);

                int ret = OK;
                if (!WarmUpUtility.UnfinishedManagement.checkAndMark(this, pathOfSo, offset)) {
                    ret = WARM_UP_FAILED_TOO_MANY_TIMES;
                } else {
                    boolean success = WarmUpDelegate.internalWarmUpSoPath(pathOfSo, offset, true);
                    if (!WeChatBacktraceNative.testLoadQut(pathOfSo, offset)) {
                        MatrixLog.w(TAG, "Warm up elf %s:%s success, but test load qut failed!", pathOfSo, offset);
                        success = false;
                    }
                    WarmUpUtility.UnfinishedManagement.result(this, pathOfSo, offset, success);
                    ret = success ? OK : WARM_UP_FAILED;
                }

                result.putInt(RESULT_OF_WARM_UP, ret);

            } else {
                MatrixLog.w(TAG, "Unknown cmd: %s", cmd);
            }

            return result;
        } finally {
            scheduleSuicide(false);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();

        if (!sHasInitiated) {
            init();
        }
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {

        if (!sHasLoaded) {
            loadLibrary(intent);
        }

        return mMessenger.getBinder();
    }
}
