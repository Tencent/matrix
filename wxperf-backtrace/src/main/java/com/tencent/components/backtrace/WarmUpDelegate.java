package com.tencent.components.backtrace;

import android.content.ContentResolver;
import android.content.Context;
import android.os.Bundle;

import com.tencent.stubs.logger.Log;

public class WarmUpDelegate {

    private final static String TAG = "Matrix.WarmUpDelegate";

    private boolean mIsolateRemote = false;

    private String mSavingPath;

    public void warmUpInIsolateProcess(boolean isolate) {
        mIsolateRemote = isolate;
    }

    public void setSavingPath(String savingPath) {

        mSavingPath = savingPath;

        WeChatBacktraceNative.setSavingPath(savingPath);
    }

    private boolean remoteWarmUpOffset(final Context context, String savingPath, String pathOfSo, int elfStartOffset) {

        final ContentResolver resolver = context.getContentResolver();

        try {

            Bundle extra = new Bundle();
            extra.putString(WarmUpProvider.EXTRA_SAVING_PATH, savingPath);
            extra.putString(WarmUpProvider.EXTRA_WARM_UP_SO_PATH, pathOfSo);
            extra.putInt(WarmUpProvider.EXTRA_WARM_UP_ELF_START_OFFSET, elfStartOffset);

            Bundle result = resolver.call(WarmUpProvider.getContentUri(context),
                    WarmUpProvider.METHOD_WARM_UP_WITH_OFFSET, null, extra);

            if (result != null && result.getInt(WarmUpProvider.RESULT_CODE) == WarmUpProvider.OK) {
                return true;
            } else {
                return false;
            }
        } catch (Throwable e) {
            Log.printStack(Log.ERROR, TAG, e);
            return false;
        }
    }

    public boolean warmUp(final Context context, String pathOfSo, int elfStartOffset) {
        Log.i(TAG, "Warm up path %s, offset %s, isolate %s", pathOfSo, elfStartOffset, mIsolateRemote);
        if (mIsolateRemote) {
            boolean ret = remoteWarmUpOffset(context, mSavingPath, pathOfSo, elfStartOffset);
            if (ret) {
                WeChatBacktraceNative.notifyWarmedUp(pathOfSo, elfStartOffset);
            }
            return ret;
        } else {
            internalWarmUpSoPath(pathOfSo, elfStartOffset);
            return true;
        }
    }

    void internalWarmUpSoPath(String pathOfSo, int elfStartOffset) {
        WeChatBacktraceNative.warmUp(pathOfSo, elfStartOffset);
    }

}
