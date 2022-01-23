package com.tencent.matrix.resource.processor

import com.tencent.matrix.resource.MemoryUtil
import com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo
import com.tencent.matrix.resource.config.ResourceConfig
import com.tencent.matrix.resource.config.SharePluginInfo
import com.tencent.matrix.resource.watcher.ActivityRefWatcher

class NativeForkAnalyzeProcessor(watcher: ActivityRefWatcher) : BaseLeakProcessor(watcher) {

    override fun process(destroyedActivityInfo: DestroyedActivityInfo): Boolean {

        val hprof = dumpStorageManager.newHprofFile()
        val activity = destroyedActivityInfo.mActivityName
        val key = destroyedActivityInfo.mKey

        val result = MemoryUtil.analyzeBlock(hprof.absolutePath, key)
        if (result.mLeakFound) {
            publishIssue(
                SharePluginInfo.IssueType.LEAK_FOUND,
                ResourceConfig.DumpMode.FORK_ANALYSE,
                activity, key, result.toString(), result.mAnalysisDurationMs.toString()
            )
        } else if (result.mFailure != null) {
            publishIssue(
                SharePluginInfo.IssueType.ERR_EXCEPTION,
                ResourceConfig.DumpMode.FORK_ANALYSE,
                activity, key, result.mFailure.toString(), "0"
            )
        }

        return true
    }
}

