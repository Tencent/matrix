package com.tencent.matrix.resource.processor;

import android.os.Build;

import com.tencent.matrix.resource.MemoryUtil;
import com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo;
import com.tencent.matrix.resource.analyzer.model.HeapDump;
import com.tencent.matrix.resource.config.ResourceConfig;
import com.tencent.matrix.resource.dumper.HprofFileManager;
import com.tencent.matrix.resource.watcher.ActivityRefWatcher;
import com.tencent.matrix.util.MatrixLog;

import java.io.File;
import java.io.FileNotFoundException;

/**
 * HPROF file dump processor using fork dump.
 *
 * @author aurorani
 * @since 2021/10/25
 */
public class ForkDumpProcessor extends BaseLeakProcessor {

    private static final String TAG = "Matrix.LeakProcessor.ForkDump";

    public ForkDumpProcessor(ActivityRefWatcher watcher) {
        super(watcher);
    }

    @Override
    public boolean process(DestroyedActivityInfo destroyedActivityInfo) {
        if (Build.VERSION.SDK_INT > ResourceConfig.FORK_DUMP_SUPPORTED_API_GUARD) {
            MatrixLog.e(TAG, "unsupported API version " + Build.VERSION.SDK_INT);
            return false;
        }

        final long dumpStart = System.currentTimeMillis();

        File hprof = null;
        try {
            hprof = HprofFileManager.INSTANCE.prepareHprofFile("FDP");
        } catch (FileNotFoundException e) {
            MatrixLog.printErrStackTrace(TAG, e , "");
        }

        if (hprof == null) {
            MatrixLog.e(TAG, "cannot create hprof file, just ignore");
            return true;
        }

        if (!MemoryUtil.dump(hprof.getPath(), 600)) {
            MatrixLog.e(TAG, String.format("heap dump for further analyzing activity with key [%s] was failed, just ignore.",
                    destroyedActivityInfo.mKey));
            return true;
        }

        MatrixLog.i(TAG, String.format("dump cost=%sms refString=%s path=%s",
                System.currentTimeMillis() - dumpStart, destroyedActivityInfo.mKey, hprof.getPath()));

        getWatcher().markPublished(destroyedActivityInfo.mActivityName);
        getWatcher().triggerGc();

        getHeapDumpHandler().process(
                new HeapDump(hprof, destroyedActivityInfo.mKey, destroyedActivityInfo.mActivityName));

        return true;
    }
}
