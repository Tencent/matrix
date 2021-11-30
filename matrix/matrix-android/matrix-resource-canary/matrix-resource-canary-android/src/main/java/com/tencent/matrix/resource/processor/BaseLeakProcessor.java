package com.tencent.matrix.resource.processor;

import android.os.Build;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.resource.CanaryWorkerService;
import com.tencent.matrix.resource.ResourcePlugin;
import com.tencent.matrix.resource.analyzer.ActivityLeakAnalyzer;
import com.tencent.matrix.resource.analyzer.model.ActivityLeakResult;
import com.tencent.matrix.resource.analyzer.model.AndroidExcludedRefs;
import com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo;
import com.tencent.matrix.resource.analyzer.model.ExcludedRefs;
import com.tencent.matrix.resource.analyzer.model.HeapDump;
import com.tencent.matrix.resource.analyzer.model.HeapSnapshot;
import com.tencent.matrix.resource.config.ResourceConfig;
import com.tencent.matrix.resource.config.SharePluginInfo;
import com.tencent.matrix.resource.dumper.AndroidHeapDumper;
import com.tencent.matrix.resource.dumper.DumpStorageManager;
import com.tencent.matrix.resource.watcher.ActivityRefWatcher;
import com.tencent.matrix.util.MatrixLog;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.IOException;

/**
 * Created by Yves on 2021/2/25
 */
public abstract class BaseLeakProcessor {
    private static final String TAG = "Matrix.LeakProcessor.Base";

    private final ActivityRefWatcher mWatcher;

    private DumpStorageManager                mDumpStorageManager;
    private AndroidHeapDumper                 mHeapDumper;
    private AndroidHeapDumper.HeapDumpHandler mHeapDumpHandler;

    public BaseLeakProcessor(ActivityRefWatcher watcher) {
        mWatcher = watcher;
    }

    public abstract boolean process(DestroyedActivityInfo destroyedActivityInfo);

    public DumpStorageManager getDumpStorageManager() {
        if (mDumpStorageManager == null) {
            mDumpStorageManager = new DumpStorageManager(mWatcher.getContext());
        }
        return mDumpStorageManager;
    }

    public AndroidHeapDumper getHeapDumper() {
        if (mHeapDumper == null) {
            mHeapDumper = new AndroidHeapDumper(mWatcher.getContext(), getDumpStorageManager());
        }
        return mHeapDumper;
    }

    protected AndroidHeapDumper.HeapDumpHandler getHeapDumpHandler() {
        if (mHeapDumpHandler == null) {
            mHeapDumpHandler = new AndroidHeapDumper.HeapDumpHandler() {
                @Override
                public void process(HeapDump result) {
                    CanaryWorkerService.shrinkHprofAndReport(mWatcher.getContext(), result);
                }
            };
        }

        return mHeapDumpHandler;
    }

    public ActivityRefWatcher getWatcher() {
        return mWatcher;
    }

    public void onDestroy() {
    }

    protected ActivityLeakResult analyze(File hprofFile, String referenceKey) {
        final HeapSnapshot heapSnapshot;
        ActivityLeakResult result;
        String manufacture = Matrix.with().getPluginByClass(ResourcePlugin.class).getConfig().getManufacture();
        final ExcludedRefs excludedRefs = AndroidExcludedRefs.createAppDefaults(Build.VERSION.SDK_INT, manufacture).build();
        try {
            heapSnapshot = new HeapSnapshot(hprofFile);
            result = new ActivityLeakAnalyzer(referenceKey, excludedRefs).analyze(heapSnapshot);
        } catch (IOException e) {
            result = ActivityLeakResult.failure(e, 0);
        }
        return result;
    }

    final protected void publishIssue(int issueType, ResourceConfig.DumpMode dumpMode, String activity, String refKey, String detail, String cost) {
        Issue issue = new Issue(issueType);
        JSONObject content = new JSONObject();
        try {
            content.put(SharePluginInfo.ISSUE_DUMP_MODE, dumpMode.name());
            content.put(SharePluginInfo.ISSUE_ACTIVITY_NAME, activity);
            content.put(SharePluginInfo.ISSUE_REF_KEY, refKey);
            content.put(SharePluginInfo.ISSUE_LEAK_DETAIL, detail);
            content.put(SharePluginInfo.ISSUE_COST_MILLIS, cost);
        } catch (JSONException jsonException) {
            MatrixLog.printErrStackTrace(TAG, jsonException, "");
        }
        issue.setContent(content);
        getWatcher().getResourcePlugin().onDetectIssue(issue);
    }
}
