package com.tencent.matrix.resource.processor

import com.tencent.matrix.resource.MemoryUtil
import com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo
import com.tencent.matrix.resource.config.ResourceConfig
import com.tencent.matrix.resource.config.SharePluginInfo
import com.tencent.matrix.resource.watcher.ActivityRefWatcher
import com.tencent.matrix.util.MatrixLog

class NativeForkAnalyzeProcessor(watcher: ActivityRefWatcher) : BaseLeakProcessor(watcher) {

    companion object {
        private const val TAG = "Matrix.LeakProcessor.NativeForkAnalyze"
    }

    override fun process(destroyedActivityInfo: DestroyedActivityInfo): Boolean {

        val hprof = dumpStorageManager.newHprofFile()
        val activity = destroyedActivityInfo.mActivityName
        val key = destroyedActivityInfo.mKey

        watcher.triggerGc()

        try {
            val result = MemoryUtil.dumpAndAnalyze(hprof.absolutePath, key, timeout = 600)
            if (result.mFailure != null) {
                publishIssue(
                    SharePluginInfo.IssueType.ERR_EXCEPTION,
                    ResourceConfig.DumpMode.FORK_ANALYSE,
                    activity, key, result.mFailure.toString(), "0"
                )
            } else {
                if (result.mLeakFound) {
                    publishIssue(
                        SharePluginInfo.IssueType.LEAK_FOUND,
                        ResourceConfig.DumpMode.FORK_ANALYSE,
                        activity, key, result.toString(), result.mAnalysisDurationMs.toString()
                    )
                }
            }
        } catch (throwable: Throwable) {
            MatrixLog.printErrStackTrace(TAG, throwable, "")
        } finally {
            if (hprof.exists()) {
                hprof.delete()
            }
        }

        return true
    }
}

