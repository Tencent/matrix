package com.tencent.matrix.resource.processor;

import com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo;
import com.tencent.matrix.resource.config.ResourceConfig;
import com.tencent.matrix.resource.config.SharePluginInfo;
import com.tencent.matrix.resource.watcher.ActivityRefWatcher;
import com.tencent.matrix.util.MatrixLog;

/**
 * Created by Yves on 2021/3/4
 */
public class NoDumpProcessor extends BaseLeakProcessor {

    private static final String TAG = "Matrix.LeakProcessor.NoDump";

    public NoDumpProcessor(ActivityRefWatcher watcher) {
        super(watcher);
    }

    @Override
    public boolean process(DestroyedActivityInfo destroyedActivityInfo) {
        // Lightweight mode, just report leaked activity name.
        MatrixLog.i(TAG, "lightweight mode, just report leaked activity name.");
        getWatcher().markPublished(destroyedActivityInfo.mActivityName);

        publishIssue(SharePluginInfo.IssueType.LEAK_FOUND, ResourceConfig.DumpMode.NO_DUMP, destroyedActivityInfo.mActivityName, destroyedActivityInfo.mKey, "no dump", "0");

        return true;
    }
}
