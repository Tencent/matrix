package com.tencent.matrix.resource.processor;

import android.os.Build;

import com.tencent.matrix.resource.MemoryUtil;
import com.tencent.matrix.resource.analyzer.model.ActivityLeakResult;
import com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo;
import com.tencent.matrix.resource.config.ResourceConfig;
import com.tencent.matrix.resource.config.SharePluginInfo;
import com.tencent.matrix.resource.watcher.ActivityRefWatcher;
import com.tencent.matrix.util.MatrixLog;

import java.io.File;

/**
 * HPROF file analysis processor using fork dump.
 *
 * @author aurorani
 * @since 2021/10/25
 */
public class ForkAnalyseProcessor extends BaseLeakProcessor {

    private static final String TAG = "Matrix.LeakProcessor.ForkAnalyse";

    public ForkAnalyseProcessor(ActivityRefWatcher watcher) {
        super(watcher);
    }

    @Override
    public boolean process(DestroyedActivityInfo destroyedActivityInfo) {
        if (Build.VERSION.SDK_INT > ResourceConfig.FORK_DUMP_SUPPORTED_API_GUARD) {
            MatrixLog.e(TAG, "cannot fork-dump with unsupported API version " + Build.VERSION.SDK_INT);
            publishIssue(
                    SharePluginInfo.IssueType.ERR_UNSUPPORTED_API,
                    ResourceConfig.DumpMode.FORK_ANALYSE,
                    destroyedActivityInfo.mActivityName, destroyedActivityInfo.mKey,
                    "Unsupported API", "0");
            return false;
        }

        getWatcher().triggerGc();

        if (dumpAndAnalyse(
                destroyedActivityInfo.mActivityName,
                destroyedActivityInfo.mKey
        )) {
            getWatcher().markPublished(destroyedActivityInfo.mActivityName, false);
            return true;
        }

        return false;
    }

    private boolean dumpAndAnalyse(String activity, String key) {

        /* Dump */

        final long dumpStart = System.currentTimeMillis();

        final File hprof = getDumpStorageManager().newHprofFile();

        if (hprof != null) {
            if (!MemoryUtil.dump(hprof.getPath(), 600)) {
                MatrixLog.e(TAG, String.format("heap dump for further analyzing activity with key [%s] was failed, just ignore.",
                        key));
                return false;
            }
        }

        if (hprof == null || hprof.length() == 0) {
            publishIssue(
                    SharePluginInfo.IssueType.ERR_FILE_NOT_FOUND,
                    ResourceConfig.DumpMode.FORK_ANALYSE,
                    activity, key, "FileNull", "0");
            MatrixLog.e(TAG, "cannot create hprof file");
            return false;
        }

        MatrixLog.i(TAG, String.format("dump cost=%sms refString=%s path=%s",
                System.currentTimeMillis() - dumpStart, key, hprof.getPath()));

        /* Analyse */

        try {
            final long analyseStart = System.currentTimeMillis();

            final ActivityLeakResult leaks = analyze(hprof, key);
            MatrixLog.i(TAG, String.format("analyze cost=%sms refString=%s",
                    System.currentTimeMillis() - analyseStart, key));

            if (leaks.mLeakFound) {
                final String leakChain = leaks.toString();
                publishIssue(
                        SharePluginInfo.IssueType.LEAK_FOUND,
                        ResourceConfig.DumpMode.FORK_ANALYSE,
                        activity, key, leakChain,
                        String.valueOf(System.currentTimeMillis() - dumpStart));
                MatrixLog.i(TAG, leakChain);
            } else {
                MatrixLog.i(TAG, "leak not found");
            }

        } catch (OutOfMemoryError error) {
            publishIssue(
                    SharePluginInfo.IssueType.ERR_ANALYSE_OOM,
                    ResourceConfig.DumpMode.FORK_ANALYSE,
                    activity, key, "OutOfMemoryError",
                    "0");
            MatrixLog.printErrStackTrace(TAG, error.getCause(), "");
        } finally {
            //noinspection ResultOfMethodCallIgnored
            hprof.delete();
        }

        /* Done */

        return true;
    }
}
