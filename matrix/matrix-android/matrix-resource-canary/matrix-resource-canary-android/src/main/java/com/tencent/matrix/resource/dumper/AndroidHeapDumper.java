/*
 * Copyright (C) 2015 Square, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.tencent.matrix.resource.dumper;

import android.content.Context;
import android.os.Debug;
import android.os.Handler;
import android.os.Looper;
import android.os.MessageQueue;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.widget.Toast;

import com.tencent.matrix.resource.leakcanary.internal.FutureResult;
import com.tencent.matrix.resource.R;
import com.tencent.matrix.resource.analyzer.model.HeapDump;
import com.tencent.matrix.util.MatrixLog;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.concurrent.TimeUnit;

/**
 * Created by tangyinsheng on 2017/6/2.
 * <p>
 * This class is ported from LeakCanary.
 */

public class AndroidHeapDumper {
    private static final String TAG = "Matrix.AndroidHeapDumper";

    private final Context mContext;
    private final Handler mMainHandler;

    public interface HeapDumpHandler {
        void process(HeapDump result);
    }

    public AndroidHeapDumper(Context context) {
        this(context, new Handler(Looper.getMainLooper()));
    }

    public AndroidHeapDumper(Context context, Handler mainHandler) {
        mContext = context;
        mMainHandler = mainHandler;
    }

    public File dumpHeap(boolean isShowToast) {
        File hprofFile = null;
        try {
            hprofFile = HprofFileManager.INSTANCE.prepareHprofFile("", false);
        } catch (FileNotFoundException e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }

        if (null == hprofFile) {
            MatrixLog.w(TAG, "hprof file is null.");
            return null;
        }

        final File hprofDir = hprofFile.getParentFile();
        if (hprofDir == null) {
            MatrixLog.w(TAG, "hprof file path: %s does not indicate a full path.", hprofFile.getAbsolutePath());
            return null;
        }

        if (!hprofDir.canWrite()) {
            MatrixLog.w(TAG, "hprof file path: %s cannot be written.", hprofFile.getAbsolutePath());
            return null;
        }

        if (hprofDir.getFreeSpace() < 1.5 * 1024 * 1024 * 1024) {
            MatrixLog.w(TAG, "hprof file path: %s free space not enough", hprofDir.getAbsolutePath());
            return null;
        }

        if (isShowToast) {
            final FutureResult<Toast> waitingForToast = new FutureResult<>();
            showToast(waitingForToast);

            if (!waitingForToast.wait(5, TimeUnit.SECONDS)) {
                MatrixLog.w(TAG, "give up dumping heap, waiting for toast too long.");
                return null;
            }
            try {
                Debug.dumpHprofData(hprofFile.getAbsolutePath());
                cancelToast(waitingForToast.get());
                return hprofFile;
            } catch (Exception e) {
                MatrixLog.printErrStackTrace(TAG, e, "failed to dump heap into file: %s.", hprofFile.getAbsolutePath());
                return null;
            }
        } else {
            try {
                Debug.dumpHprofData(hprofFile.getAbsolutePath());
                return hprofFile;
            } catch (Exception e) {
                MatrixLog.printErrStackTrace(TAG, e, "failed to dump heap into file: %s.", hprofFile.getAbsolutePath());
                return null;
            }
        }
    }

    private void showToast(final FutureResult<Toast> waitingForToast) {
        mMainHandler.post(new Runnable() {
            @Override
            public void run() {
                final Toast toast = new Toast(mContext);
                toast.setDuration(Toast.LENGTH_LONG);
                toast.setGravity(Gravity.CENTER_VERTICAL, 0, 0);
                LayoutInflater inflater = LayoutInflater.from(mContext);
                toast.setView(inflater.inflate(R.layout.resource_canary_toast_wait_for_heapdump, null));
                toast.show();
                // Waiting for Idle to make sure Toast gets rendered.
                Looper.myQueue().addIdleHandler(new MessageQueue.IdleHandler() {
                    @Override
                    public boolean queueIdle() {
                        waitingForToast.set(toast);
                        return false;
                    }
                });
            }
        });
    }

    private void cancelToast(final Toast toast) {
        mMainHandler.post(new Runnable() {
            @Override
            public void run() {
                toast.cancel();
            }
        });
    }
}
