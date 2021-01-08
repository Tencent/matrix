package com.tencent.components.backtrace;

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
import android.support.annotation.Nullable;

import com.tencent.stubs.logger.Log;

import java.util.concurrent.atomic.AtomicInteger;

public class WarmUpService extends Service {

    private final static String TAG = "Matrix.WarmUp.WarmUpService";

    interface RemoteInvoker {
        Bundle call(int cmd, Bundle args);
    }

    interface RemoteConnection {
        boolean connect(Context context);
        void disconnect(Context context);
    }

    private final static String INVOKE_ARGS = "invoke-args";
    private final static String INVOKE_RESP = "invoke-resp";

    final static class RemoteInvokerImpl implements RemoteInvoker, RemoteConnection {

        private final static String TAG = "Matrix.WarmUp.RemoteInvoker";

        volatile Messenger mResp;
        volatile Messenger mReq;

        volatile Bundle[] mResult = {null};
        HandlerThread[] mHandlerThread = {null};

        ServiceConnection mConnection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                mReq = new Messenger(service);
                synchronized (mBound) {
                    mBound[0] = true;
                    mBound.notifyAll();
                }
                Log.i(TAG, "This remote invoker(%s) connected.", this);
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {
                mReq = null;
                synchronized (mBound) {
                    mBound[0] = false;
                }
                Log.i(TAG, "This remote invoker(%s) disconnected.", this);
            }
        };

        private volatile boolean[] mBound = {false};

        private void checkThread() {
            if (Looper.getMainLooper() == Looper.myLooper()) {
                throw new RuntimeException("Should not call this from main thread!");
            }
        }

        @Override
        public boolean connect(Context context) {

            checkThread();

            if (mBound[0]) {
                return true;
            }

            Log.i(TAG, "Start connecting to remote. (%s)", this.hashCode());

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
            context.bindService(intent, mConnection, BIND_AUTO_CREATE);

            try {
                synchronized (mBound) {
                    mBound.wait(20000); // 20s
                }
            } catch (InterruptedException e) {
                Log.printStack(Log.ERROR, TAG, e);
            }

            if (!mBound[0]) {
                context.unbindService(mConnection);
            }

            return mBound[0];

        }

        @Override
        public void disconnect(Context context) {

            Log.i(TAG, "Start disconnecting to remote. (%s)", this.hashCode());

            synchronized (mHandlerThread) {
                if (mHandlerThread[0] != null) {
                    mHandlerThread[0].quitSafely();
                    mHandlerThread[0] = null;
                }
            }

            if (mBound[0]) {
                context.unbindService(mConnection);
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
                        mResult.wait(30000); // 30s
                        return mResult[0];
                    }
                }
            } catch (RemoteException e) {
                Log.printStack(Log.ERROR, TAG, e);
            } catch (InterruptedException e) {
                Log.printStack(Log.ERROR, TAG, e);
            }

            return null;
        }
    }

    final static String ARGS_WARM_UP_SAVING_PATH = "saving-path";
    final static String ARGS_WARM_UP_PATH_OF_ELF = "path-of-elf";
    final static String ARGS_WARM_UP_ELF_START_OFFSET = "elf-start-offset";
    final static String RESULT_OF_WARM_UP = "warm-up-result";

    final static int CMD_WARM_UP_SINGLE_ELF_FILE = 100;

    private Messenger mMessenger = new Messenger(new Handler() {
        @Override
        public void handleMessage(Message msg) {
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
                    Log.printStack(Log.ERROR, TAG, e);
                }
            }
        }
    });

    final static int OK = 0;
    public static final int INVALID_ARGUMENT = -1;

    private static volatile boolean sHasInitiated = false;
    private static HandlerThread mRecycler;
    private static Handler sRecyclerHandler;
    private static AtomicInteger sWorkingCall = new AtomicInteger(0);
    private final static byte[] sRecyclerLock = new byte[0];
    private WarmUpDelegate mWarmUpDelegate = new WarmUpDelegate();
    private final static int MSG_SUICIDE = 1;
    private final static long INTERVAL_OF_CHECK = 60 * 1000; // ms

    private final static class RecyclerCallback implements Handler.Callback {

        @Override
        public boolean handleMessage(Message msg) {
            if (msg.what == MSG_SUICIDE) {
                Log.i(TAG, "Suicide.");
                android.os.Process.killProcess(android.os.Process.myPid());
                System.exit(0);
            }
            return false;
        }
    }

    private synchronized static void init() {

        if (sHasInitiated) {
            return;
        }

        Log.i(TAG, "Init called.");
        WeChatBacktrace.loadLibrary();

        synchronized (sRecyclerLock) {
            if (mRecycler == null) {
                mRecycler = new HandlerThread("wechat-backtrace-proc-recycler");
                mRecycler.start();
                sRecyclerHandler = new Handler(mRecycler.getLooper(), new RecyclerCallback());
            }
        }

        scheduleSuicide(true);

        sHasInitiated = true;
    }

    private boolean isNullOrNil(String string) {
        return string == null || string.isEmpty();
    }

    private static void scheduleSuicide(boolean onCreate) {
        Log.i(TAG, "Schedule suicide");
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
        Log.i(TAG, "Remove scheduled suicide");
        synchronized (sRecyclerLock) {
            sRecyclerHandler.removeMessages(MSG_SUICIDE);
            sWorkingCall.getAndIncrement();
        }
    }

    private Bundle call(int cmd, Bundle args) {

        if (!sHasInitiated) {
            init();
        }

        removeScheduledSuicide();
        try {
            final Bundle result = new Bundle();
            result.putInt(RESULT_OF_WARM_UP, INVALID_ARGUMENT);
            Log.i(TAG, "Invoke from client with args: %s.", args);

            String savingPath = args.getString(ARGS_WARM_UP_SAVING_PATH, null);
            if (isNullOrNil(savingPath)) {
                Log.i(TAG, "Saving path is empty.");
                return result;
            }
            mWarmUpDelegate.setSavingPath(savingPath);

            if (cmd == CMD_WARM_UP_SINGLE_ELF_FILE) {

                String pathOfSo = args.getString(ARGS_WARM_UP_PATH_OF_ELF, null);
                if (isNullOrNil(pathOfSo)) {
                    Log.i(TAG, "Warm-up so path is empty.");
                    return result;
                }
                int offset = args.getInt(ARGS_WARM_UP_ELF_START_OFFSET, 0);

                Log.i(TAG, "Warm up so path %s offset %s.", pathOfSo, offset);
                mWarmUpDelegate.internalWarmUpSoPath(pathOfSo, offset);
                result.putInt(RESULT_OF_WARM_UP, OK);

            } else {
                Log.w(TAG, "Unknown cmd: %s", cmd);
            }

            return result;
        } finally {
            scheduleSuicide(false);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        init();
        scheduleSuicide(true);
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return mMessenger.getBinder();
    }
}
