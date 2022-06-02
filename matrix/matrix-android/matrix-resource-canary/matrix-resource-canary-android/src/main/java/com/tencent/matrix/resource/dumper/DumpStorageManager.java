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
import android.os.Environment;
import android.os.Process;

import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import java.io.File;
import java.io.FilenameFilter;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * Created by tangyinsheng on 2017/6/2.
 * <p>
 * This class is ported from LeakCanary.
 */
@Deprecated
public class DumpStorageManager {
    private static final String TAG = "Matrix.DumpStorageManager";

    public static final String HPROF_EXT = ".hprof";

    public static final int DEFAULT_MAX_STORED_HPROF_FILECOUNT = 5;

    protected final Context mContext;
    protected final int mMaxStoredHprofFileCount;

    public DumpStorageManager(Context context) {
        this(context, DEFAULT_MAX_STORED_HPROF_FILECOUNT);
    }

    public DumpStorageManager(Context context, int maxStoredHprofFileCount) {
        if (maxStoredHprofFileCount <= 0) {
            throw new IllegalArgumentException("illegal max stored hprof file count: "
                    + maxStoredHprofFileCount);
        }
        mContext = context;
        mMaxStoredHprofFileCount = maxStoredHprofFileCount;
    }

    public File newHprofFile() {
        final File storageDir = prepareStorageDirectory();
        if (storageDir == null) {
            return null;
        }
        final String hprofFileName = "dump"
                + "_" + MatrixUtil.getProcessName(mContext).replace(".", "_").replace(":", "_")
                + "_" + Process.myPid()
                + "_"
                + new SimpleDateFormat("yyyyMMddHHmmss", Locale.getDefault()).format(new Date())
                + HPROF_EXT;
        return new File(storageDir, hprofFileName);
    }

    private File prepareStorageDirectory() {
        final File storageDir = getStorageDirectory();
        if (!storageDir.exists() && (!storageDir.mkdirs() || !storageDir.canWrite())) {
            MatrixLog.w(TAG, "failed to allocate new hprof file since path: %s is not writable.",
                    storageDir.getAbsolutePath());
            return null;
        }
        final File[] hprofFiles = storageDir.listFiles(new FilenameFilter() {
            @Override
            public boolean accept(File dir, String name) {
                return name.endsWith(HPROF_EXT);
            }
        });
        if (hprofFiles != null && hprofFiles.length > mMaxStoredHprofFileCount) {
            for (File file : hprofFiles) {
                if (file.exists() && !file.delete()) {
                    MatrixLog.w(TAG, "faile to delete hprof file: " + file.getAbsolutePath());
                }
            }
        }
        return storageDir;
    }

    private File getStorageDirectory() {
        final String sdcardState = Environment.getExternalStorageState();
        File root = null;
        if (Environment.MEDIA_MOUNTED.equals(sdcardState)) {
            root = mContext.getExternalCacheDir();
        } else {
            root = mContext.getCacheDir();
        }
        final File result = new File(root, "matrix_resource");

        MatrixLog.i(TAG, "path to store hprof and result: %s", result.getAbsolutePath());

        return result;
    }
}
