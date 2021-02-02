package com.tencent.components.backtrace;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.PowerManager;

import com.tencent.stubs.logger.Log;

import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

public class WarmUpScheduler implements Handler.Callback {

    private final static String TAG = "Matrix.WarmUpScheduler";

    final static long DELAY_SHORTLY = 3 * 1000;
    final static long DELAY_CLEAN_UP = DELAY_SHORTLY;
    final static long DELAY_WARM_UP = DELAY_SHORTLY;
    final static long DELAY_CONSUME_REQ_QUT = DELAY_SHORTLY;

    private final static int MSG_WARM_UP = 1;
    private final static int MSG_CONSUME_REQ_QUT = 2;
    private final static int MSG_CLEAN_UP = 3;
    private final static int MSG_COMPUTE_DISK_USAGE = 4;

    private IdleReceiver mIdleReceiver;
    private WarmUpDelegate mDelegate;
    private Handler mHandler;
    private Context mContext;

    private WeChatBacktrace.WarmUpTiming mTiming;
    private long mWarmUpDelay = 0;

    enum TaskType {
        WarmUp,
        CleanUp,
        RequestConsuming,
        DiskUsage,
    }

    WarmUpScheduler(WarmUpDelegate delegate, Context context, WeChatBacktrace.WarmUpTiming timing, long delay) {
        mDelegate = delegate;

        if (mHandler == null) {
            mHandler = new Handler(Looper.getMainLooper(), this);
        }

        mContext = context;

        mTiming = timing;
        mWarmUpDelay = Math.max(delay, DELAY_SHORTLY);
    }

    void scheduleTask(TaskType type) {
        switch (mTiming) {
            case PostStartup:
                arrangeTaskDirectly(type);
                break;
            case WhileCharging:
            case WhileScreenOff:
                arrangeTaskToIdleReceiver(mContext, type);
                break;
            default:
                break;
        }
    }

    void taskFinished(TaskType type) {
        switch (mTiming) {
            case WhileCharging:
            case WhileScreenOff:
                finishTaskToIdleReceiver(mContext, type);
                break;
            default:
                break;
        }
    }

    private void arrangeTaskDirectly(TaskType type) {
        switch (type) {
            case WarmUp:
                Log.i(TAG, "Schedule warm-up in %ss", mWarmUpDelay / 1000);
                mHandler.sendMessageDelayed(
                        Message.obtain(mHandler, MSG_WARM_UP, new CancellationSignal()),
                        mWarmUpDelay);
                break;
            case CleanUp:
                Log.i(TAG, "Schedule clean-up in %ss", mWarmUpDelay / 1000);
                mHandler.sendMessageDelayed(
                        Message.obtain(mHandler, MSG_CLEAN_UP, new CancellationSignal()),
                        mWarmUpDelay);
                break;
            case RequestConsuming:
                Log.i(TAG, "Schedule request consuming in %ss", mWarmUpDelay / 1000);
                mHandler.sendMessageDelayed(
                        Message.obtain(mHandler, MSG_CONSUME_REQ_QUT, new CancellationSignal()),
                        mWarmUpDelay);
                break;
        }
    }

    private synchronized void arrangeTaskToIdleReceiver(Context context, TaskType type) {
        if (mIdleReceiver == null) {
            mIdleReceiver = new IdleReceiver(context, mHandler, mTiming, mWarmUpDelay);
            mIdleReceiver.arrange(type);
        } else {
            mIdleReceiver.arrange(type);
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

    private synchronized void finishTaskToIdleReceiver(Context context, TaskType type) {
        if (mIdleReceiver != null) {
            if (mIdleReceiver.finish(type) == 0) {
                Log.i(TAG, "Unregister idle receiver.");
                context.unregisterReceiver(mIdleReceiver);
                mIdleReceiver = null;
            }
        }
    }

    @Override
    public boolean handleMessage(Message msg) {
        switch (msg.what) {
            case MSG_WARM_UP:
            {
                CancellationSignal cs = (CancellationSignal) msg.obj;
                mDelegate.warmingUp(cs);
                break;
            }
            case MSG_CONSUME_REQ_QUT: {
                CancellationSignal cs = (CancellationSignal) msg.obj;
                mDelegate.consumingRequestedQut(cs);
                break;
            }
            case MSG_CLEAN_UP: {
                CancellationSignal cs = (CancellationSignal) msg.obj;
                mDelegate.cleaningUp(cs);
                break;
            }
            case MSG_COMPUTE_DISK_USAGE: {
                CancellationSignal cs = (CancellationSignal) msg.obj;
                mDelegate.computeDiskUsage(cs);
                break;
            }
        }
        return false;
    }

    private final static class IdleReceiver extends BroadcastReceiver {

        private CancellationSignal mCancellationSignal;

        Handler mIdleHandler;

        Context mContext;
        private WeChatBacktrace.WarmUpTiming mTiming;
        private long mWarmUpDelay;

        private Set<TaskType> mTasks = new HashSet<>();

        IdleReceiver(Context context, Handler idleHandler, WeChatBacktrace.WarmUpTiming timing,
                            long delay) {
            super();
            mContext = context;
            mIdleHandler = idleHandler;
            mTiming = timing;
            mWarmUpDelay = delay;
        }

        synchronized void arrange(TaskType type) {
            if (mTasks.contains(type)) {
                return;
            }
            mTasks.add(type);
        }

         synchronized int finish(TaskType type) {
            mTasks.remove(type);
            return mTasks.size();
        }

        private synchronized void triggerIdle(boolean isInteractive, boolean isCharging) {
            Log.i(TAG, "Idle status changed: interactive = %s, charging = %s",
                    isInteractive, isCharging);

            boolean isIdle = (!isInteractive) &&
                    (mTiming == WeChatBacktrace.WarmUpTiming.WhileScreenOff || !isCharging);

            if (isIdle && mCancellationSignal == null) {
                mCancellationSignal = new CancellationSignal();
                Iterator<TaskType> it = mTasks.iterator();
                while (it.hasNext()) {
                    TaskType type = it.next();
                    switch (type) {
                        case WarmUp:
                            if (!WarmUpUtility.hasWarmedUp(mContext)) {
                                mIdleHandler.sendMessageDelayed(
                                        Message.obtain(mIdleHandler, MSG_WARM_UP, mCancellationSignal),
                                        mWarmUpDelay
                                );
                                Log.i(TAG, "System idle, trigger warm up in %s seconds.", (mWarmUpDelay / 1000));
                            } else {
                                it.remove();
                            }
                            break;
                        case RequestConsuming:
                            mIdleHandler.sendMessageDelayed(
                                    Message.obtain(mIdleHandler, MSG_CONSUME_REQ_QUT, mCancellationSignal),
                                    mWarmUpDelay
                            );
                            Log.i(TAG, "System idle, trigger consume requested qut in %s seconds.", (mWarmUpDelay / 1000));
                            break;
                        case CleanUp:
                            if (WarmUpUtility.needCleanUp(mContext)) {
                                mIdleHandler.sendMessageDelayed(
                                        Message.obtain(mIdleHandler, MSG_CLEAN_UP, mCancellationSignal),
                                        DELAY_CLEAN_UP
                                );
                            } else {
                                it.remove();
                            }
                            Log.i(TAG, "System idle, trigger clean up in %s seconds.", (DELAY_CLEAN_UP / 1000));
                            break;
                        case DiskUsage:
                            if (WarmUpUtility.shouldComputeDiskUsage(mContext)) {
                                mIdleHandler.sendMessageDelayed(
                                        Message.obtain(mIdleHandler, MSG_COMPUTE_DISK_USAGE, mCancellationSignal),
                                        DELAY_SHORTLY
                                );
                            } else {
                                it.remove();
                            }
                            Log.i(TAG, "System idle, trigger disk usage in %s seconds.", (DELAY_SHORTLY / 1000));
                            break;
                    }
                }
            } else if (!isIdle && mCancellationSignal != null) {
                mIdleHandler.removeMessages(MSG_WARM_UP);
                mIdleHandler.removeMessages(MSG_CONSUME_REQ_QUT);
                mIdleHandler.removeMessages(MSG_CLEAN_UP);
                mIdleHandler.removeMessages(MSG_COMPUTE_DISK_USAGE);
                mCancellationSignal.cancel();
                mCancellationSignal = null;
                Log.i(TAG, "Exit idle state, task cancelled.");
            }
        }

        synchronized void refreshIdleStatus(Context context) {
            PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
            boolean isInteractive = pm.isInteractive();
            boolean isCharging = false;

            IntentFilter filter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
            Intent intent = context.registerReceiver(null, filter);
            if (intent != null) {
                int status = intent.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
                isCharging = (status == BatteryManager.BATTERY_STATUS_CHARGING) ||
                        (status == BatteryManager.BATTERY_STATUS_FULL);
            }
            triggerIdle(isInteractive, isCharging);
        }

        @Override
        public void onReceive(Context context, final Intent intent) {
            String action = intent.getAction();
            if (action == null) return;

            boolean isInteractive = false;
            boolean isCharging = false;

            synchronized (this) {
                switch (action) {
                    case Intent.ACTION_SCREEN_ON:
                        isInteractive = true;
                        break;
                    case Intent.ACTION_SCREEN_OFF:
                        isInteractive = false;
                        break;
                    case Intent.ACTION_POWER_CONNECTED:
                        isCharging = true;
                        break;
                    case Intent.ACTION_POWER_DISCONNECTED:
                        isCharging = false;
                        break;
                }
                triggerIdle(isInteractive, isCharging);
            }
        }
    }

}
