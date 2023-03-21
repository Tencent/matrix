package com.tencent.matrix.resource.processor;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import com.tencent.matrix.resource.analyzer.model.ActivityLeakResult;
import com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo;
import com.tencent.matrix.resource.config.ResourceConfig;
import com.tencent.matrix.resource.config.SharePluginInfo;
import com.tencent.matrix.resource.watcher.ActivityRefWatcher;
import com.tencent.matrix.util.MatrixLog;

import java.io.File;

/**
 * Created by Yves on 2021/2/25
 */
public class SilenceAnalyseProcessor extends BaseLeakProcessor {

    private static final String TAG = "Matrix.LeakProcessor.SilenceAnalyse";

    private final BroadcastReceiver mReceiver;

    private boolean isScreenOff;
    private boolean isProcessing;

    public SilenceAnalyseProcessor(ActivityRefWatcher watcher) {
        super(watcher);

        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        filter.addAction(Intent.ACTION_SCREEN_ON);

        mReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (Intent.ACTION_SCREEN_OFF.equals(intent.getAction())) {
                    isScreenOff = true;
                    MatrixLog.i(TAG, "[ACTION_SCREEN_OFF]");
                } else if (Intent.ACTION_SCREEN_ON.equals(intent.getAction())) {
                    isScreenOff = false;
                    MatrixLog.i(TAG, "[ACTION_SCREEN_ON]");
                }
            }
        };

        try {
            watcher.getResourcePlugin().getApplication().registerReceiver(mReceiver, filter);
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }
    }

    @Override
    public boolean process(DestroyedActivityInfo destroyedActivityInfo) {
        publishIssue(SharePluginInfo.IssueType.LEAK_FOUND, ResourceConfig.DumpMode.NO_DUMP, destroyedActivityInfo.mActivityName, destroyedActivityInfo.mKey, "no dump", "0");
        return onLeak(destroyedActivityInfo.mActivityName, destroyedActivityInfo.mKey);
    }

    @Override
    public void onDestroy() {
        MatrixLog.i(TAG, "onDestroy: unregister receiver");
        getWatcher().getResourcePlugin().getApplication().unregisterReceiver(mReceiver);
    }

    private boolean onLeak(final String activity, final String refString) {
        MatrixLog.i(TAG, "[onLeak] activity=%s isScreenOff=%s isProcessing=%s", activity, isScreenOff, isProcessing);

        if (getWatcher().isPublished(activity)) {
            MatrixLog.i(TAG, "this activity has been dumped! %s", activity);
            return true;
        }

        if (!isProcessing && isScreenOff) {
            isProcessing = true;

            getWatcher().triggerGc();

            boolean res = dumpAndAnalyse(activity, refString);

            if (res) {
                getWatcher().markPublished(activity, false);
            }

            isProcessing = false;

            return res;
        }
        return false;
    }

    // todo analyse Service
    // todo batch analyse
    private boolean dumpAndAnalyse(String activity, String refString) {

        long dumpBegin = System.currentTimeMillis();

        File file = getHeapDumper().dumpHeap(false);

        if (file == null || file.length() <= 0) {
            publishIssue(SharePluginInfo.IssueType.ERR_FILE_NOT_FOUND, activity, refString, "file is null", "0");
            MatrixLog.e(TAG, "file is null!");
            return true;
        }

        MatrixLog.i(TAG, String.format("dump cost=%sms refString=%s path=%s",
                System.currentTimeMillis() - dumpBegin, refString, file.getAbsolutePath()));

        long analyseBegin = System.currentTimeMillis();
        try {
            final ActivityLeakResult result = analyze(file, refString);
            MatrixLog.i(TAG, String.format("analyze cost=%sms refString=%s",
                    System.currentTimeMillis() - analyseBegin, refString));

            String refChain = result.toString();
            if (result.mLeakFound) {
                publishIssue(SharePluginInfo.IssueType.LEAK_FOUND, activity, refString, refChain, String.valueOf(
                        System.currentTimeMillis() - dumpBegin));
                MatrixLog.i(TAG, refChain);
            } else {
                MatrixLog.i(TAG, "leak not found");
            }
        } catch (OutOfMemoryError error) {
            publishIssue(SharePluginInfo.IssueType.ERR_ANALYSE_OOM, activity, refString, "OutOfMemoryError", "0");
            MatrixLog.printErrStackTrace(TAG, error.getCause(), "");
        } finally {
            file.delete();
        }
        return true;
    }

    private void publishIssue(int issueType, String activity, String refKey, String detail, String cost) {
        publishIssue(issueType, ResourceConfig.DumpMode.SILENCE_ANALYSE, activity, refKey, detail, cost);
    }
}
