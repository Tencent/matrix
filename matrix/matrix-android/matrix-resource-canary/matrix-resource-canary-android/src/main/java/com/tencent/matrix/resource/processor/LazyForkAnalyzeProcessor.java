package com.tencent.matrix.resource.processor;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;

import com.tencent.matrix.resource.MemoryUtil;
import com.tencent.matrix.resource.analyzer.model.ActivityLeakResult;
import com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo;
import com.tencent.matrix.resource.config.ResourceConfig;
import com.tencent.matrix.resource.config.SharePluginInfo;
import com.tencent.matrix.resource.dumper.HprofFileManager;
import com.tencent.matrix.resource.watcher.ActivityRefWatcher;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.Queue;
import java.util.concurrent.LinkedBlockingQueue;

/**
 * Difference between this process and {@link ForkAnalyseProcessor} is analyzing HPROF until the
 * screen is off.
 *
 * @author aurorani
 * @since 2021/10/25
 */
public class LazyForkAnalyzeProcessor extends BaseLeakProcessor {

    private static final String TAG = "Matrix.LeakProcessor.LazyForkAnalyze";

    private static class AnalyzeTask {
        private final File hprof;
        private final String referenceActivity;
        private final String referenceKey;
        private final long dumpStart;

        AnalyzeTask(File hprof, String referenceActivity, String referenceKey, long dumpStart) {
            this.hprof = hprof;
            this.referenceActivity = referenceActivity;
            this.referenceKey = referenceKey;
            this.dumpStart = dumpStart;
        }

        public File getHprof() {
            return hprof;
        }

        public String getReferenceActivity() {
            return referenceActivity;
        }

        public String getReferenceKey() {
            return referenceKey;
        }

        public long getDumpStart() {
            return dumpStart;
        }
    }

    private volatile boolean isInBackground = false;

    private final Queue<AnalyzeTask> lazyTasks = new LinkedBlockingQueue<>();

    private final Runnable analyzeProcessTask = new Runnable() {
        @Override
        public void run() {
            MatrixLog.v(TAG, "analyze task start. background: " + isInBackground + ", queue empty: " + lazyTasks.isEmpty());
            while (isInBackground) {
                final AnalyzeTask task = lazyTasks.poll();
                if (task == null) {
                    MatrixLog.v(TAG, "task queue is cleared");
                    break;
                }
                analyze(task);
            }
            MatrixLog.v(TAG, "analyze task complete. background: " + isInBackground + ", queue empty: " + lazyTasks.isEmpty());
        }
    };

    private final BroadcastReceiver receiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (Intent.ACTION_SCREEN_OFF.equals(intent.getAction())) {
                MatrixLog.v(TAG, "action screen off");
                isInBackground = true;
                MatrixHandlerThread.getDefaultHandler().post(analyzeProcessTask);
            } else if (Intent.ACTION_SCREEN_ON.equals(intent.getAction())) {
                MatrixLog.v(TAG, "action screen on");
                isInBackground = false;
                MatrixHandlerThread.getDefaultHandler().removeCallbacks(analyzeProcessTask);
            }
        }
    };

    public LazyForkAnalyzeProcessor(ActivityRefWatcher watcher) {
        super(watcher);
        final IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        watcher.getResourcePlugin().getApplication().registerReceiver(receiver, filter);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        getWatcher().getResourcePlugin().getApplication().unregisterReceiver(receiver);
    }

    @Override
    public boolean process(DestroyedActivityInfo destroyedActivityInfo) {
        publishIssue(SharePluginInfo.IssueType.LEAK_FOUND, ResourceConfig.DumpMode.NO_DUMP, destroyedActivityInfo.mActivityName, destroyedActivityInfo.mKey, "no dump", "0");

        if (Build.VERSION.SDK_INT > ResourceConfig.FORK_DUMP_SUPPORTED_API_GUARD) {
            MatrixLog.e(TAG, "cannot fork-dump with unsupported API version " + Build.VERSION.SDK_INT);
            publishIssue(
                    SharePluginInfo.IssueType.ERR_UNSUPPORTED_API,
                    ResourceConfig.DumpMode.LAZY_FORK_ANALYZE,
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

        final long dumpStart = System.currentTimeMillis();

        File hprof = null;
        try {
            hprof = HprofFileManager.INSTANCE.prepareHprofFile("LFAP", true);
        } catch (FileNotFoundException e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }

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
                    ResourceConfig.DumpMode.LAZY_FORK_ANALYZE,
                    activity, key, "FileNull", "0");
            MatrixLog.e(TAG, "cannot create hprof file");
            return false;
        }

        MatrixLog.i(TAG, String.format("dump cost=%sms refString=%s path=%s",
                System.currentTimeMillis() - dumpStart, key, hprof.getPath()));

        MatrixLog.i(TAG, "dump complete, push task into lazy analyze task queue");

        // Add lazy task.
        lazyTasks.add(new AnalyzeTask(hprof, activity, key, dumpStart));

        return true;
    }

    private void analyze(AnalyzeTask task) {
        try {
            final long analyseStart = System.currentTimeMillis();

            final ActivityLeakResult leaks = analyze(task.getHprof(), task.getReferenceKey());
            MatrixLog.i(TAG, String.format("analyze cost=%sms refString=%s",
                    System.currentTimeMillis() - analyseStart, task.getReferenceKey()));

            if (leaks.mLeakFound) {
                final String leakChain = leaks.toString();
                publishIssue(
                        SharePluginInfo.IssueType.LEAK_FOUND,
                        ResourceConfig.DumpMode.LAZY_FORK_ANALYZE,
                        task.getReferenceActivity(), task.getReferenceKey(), leakChain,
                        String.valueOf(System.currentTimeMillis() - task.getDumpStart()));
                MatrixLog.i(TAG, leakChain);
            } else {
                MatrixLog.e(TAG, "leak not found");
            }

        } catch (OutOfMemoryError error) {
            publishIssue(
                    SharePluginInfo.IssueType.ERR_ANALYSE_OOM,
                    ResourceConfig.DumpMode.LAZY_FORK_ANALYZE,
                    task.getReferenceActivity(), task.getReferenceKey(), "OutOfMemoryError",
                    "0");
            MatrixLog.printErrStackTrace(TAG, error.getCause(), "");
        } finally {
            MatrixLog.i(TAG, "analyze complete");
            //noinspection ResultOfMethodCallIgnored
            task.getHprof().delete();
        }
    }
}
