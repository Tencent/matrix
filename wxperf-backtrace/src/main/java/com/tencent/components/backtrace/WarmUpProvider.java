package com.tencent.components.backtrace;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import com.tencent.stubs.logger.Log;

import java.util.concurrent.atomic.AtomicInteger;

public class WarmUpProvider extends ContentProvider {

    private final static String TAG = "Matrix.WarmUp";

    private static final String AUTHORITY_SUFFIX = ".com.tencent.components.backtrace.WarmUpProvider";

    public static final String METHOD_WARM_UP_WITH_OFFSET = "warm-up-offset";

    public static final String EXTRA_SAVING_PATH = "extra-warm-up-saving-path";
    public static final String EXTRA_WARM_UP_SO_PATH = "extra-warm-up-so-path";
    public static final String EXTRA_WARM_UP_ELF_START_OFFSET = "extra-warm-up-elf-start-offset";

    public static final String RESULT_CODE = "warm-up-result";

    public static final int INVALID_ARGUMENT = -1;
    public static final int OK = -1;

    private static HandlerThread mRecycler;

    private static Handler sRecyclerHandler;

    private static AtomicInteger sWorkingCall = new AtomicInteger(0);

    private final static byte[] sRecyclerLock = new byte[0];

    private WarmUpDelegate mWarmUpDelegate = new WarmUpDelegate();

    public static String getAuthority(final Context context) {
        return context.getPackageName() + AUTHORITY_SUFFIX;
    }

    public static Uri getContentUri(final Context context) {
        return Uri.parse("content://" + getAuthority(context));
    }

    private final static int MSG_SUICIDE = 1;

    private final static long INTERVAL_OF_CHECK = 60 * 1000; // ms

    private final static class RecyclerCallback implements Handler.Callback {

        @Override
        public boolean handleMessage(Message msg) {
            if (msg.what == MSG_SUICIDE) {
                android.os.Process.killProcess(android.os.Process.myPid());
                System.exit(0);
            }
            return false;
        }
    }

    @Override
    public boolean onCreate() {
        Log.i(TAG, "onCreate called.");
        WeChatBacktrace.loadLibrary();

        synchronized (sRecyclerLock) {
            if (mRecycler == null) {
                mRecycler = new HandlerThread("wechat-backtrace-proc-recycler");
                mRecycler.start();
                sRecyclerHandler = new Handler(mRecycler.getLooper(), new RecyclerCallback());
            }
        }

        scheduleSuicide(true);

        return false;
    }

    private boolean isNullOrNil(String string) {
        return string == null || string.isEmpty();
    }

    private void scheduleSuicide(boolean onCreate) {
        Log.i(TAG, "Schedule suicide");
        if (onCreate) {
            sRecyclerHandler.sendEmptyMessageDelayed(MSG_SUICIDE, INTERVAL_OF_CHECK);
        } else {
            if (sWorkingCall.decrementAndGet() == 0) {
                sRecyclerHandler.sendEmptyMessageDelayed(MSG_SUICIDE, INTERVAL_OF_CHECK);
            }
        }
    }

    private void removeScheduledSuicide() {
        Log.i(TAG, "Remove scheduled suicide");
        sRecyclerHandler.removeMessages(MSG_SUICIDE);
        sWorkingCall.getAndIncrement();
    }

    @Nullable
    @Override
    public Bundle call(@NonNull String method, @Nullable String arg, @Nullable Bundle extras) {

        removeScheduledSuicide();
        try {
            final Bundle result = new Bundle();
            result.putInt(RESULT_CODE, INVALID_ARGUMENT);
            Log.i(TAG, "Invoke method: %s, with arg: %s, extras: %s", method, arg, extras);

            String savingPath = extras.getString(EXTRA_SAVING_PATH, null);
            if (isNullOrNil(savingPath)) {
                Log.i(TAG, "Saving path is empty.");
                return result;
            }
            mWarmUpDelegate.setSavingPath(savingPath);

            if (METHOD_WARM_UP_WITH_OFFSET.equals(method)) {

                String pathOfSo = extras.getString(EXTRA_WARM_UP_SO_PATH, null);
                if (isNullOrNil(pathOfSo)) {
                    Log.i(TAG, "Warm-up so path is empty.");
                    return result;
                }
                int offset = extras.getInt(EXTRA_WARM_UP_ELF_START_OFFSET, 0);

                Log.i(TAG, "Warm up so path %s offset %s.", pathOfSo, offset);
                mWarmUpDelegate.internalWarmUpSoPath(pathOfSo, offset);
                result.putInt(RESULT_CODE, OK);

            } else {
                Log.w(TAG, "Unknown method: %s", method);
            }

            return result;
        } finally {
            scheduleSuicide(false);
        }
    }

    @Nullable
    @Override
    public Cursor query(@NonNull Uri uri, @Nullable String[] projection, @Nullable String selection, @Nullable String[] selectionArgs, @Nullable String sortOrder) {
        return null;
    }

    @Nullable
    @Override
    public String getType(@NonNull Uri uri) {
        return null;
    }

    @Nullable
    @Override
    public Uri insert(@NonNull Uri uri, @Nullable ContentValues values) {
        return null;
    }

    @Override
    public int delete(@NonNull Uri uri, @Nullable String selection, @Nullable String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(@NonNull Uri uri, @Nullable ContentValues values, @Nullable String selection, @Nullable String[] selectionArgs) {
        return 0;
    }
}
