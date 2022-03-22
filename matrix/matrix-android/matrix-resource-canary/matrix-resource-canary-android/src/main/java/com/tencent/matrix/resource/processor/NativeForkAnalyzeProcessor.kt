package com.tencent.matrix.resource.processor

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import com.tencent.matrix.resource.MemoryUtil
import com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo
import com.tencent.matrix.resource.config.ResourceConfig
import com.tencent.matrix.resource.config.SharePluginInfo
import com.tencent.matrix.resource.watcher.ActivityRefWatcher
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import java.io.File
import java.util.*

private fun File.deleteIfExist() {
    if (exists()) delete()
}

class NativeForkAnalyzeProcessor(watcher: ActivityRefWatcher) : BaseLeakProcessor(watcher) {

    companion object {
        private const val TAG = "Matrix.LeakProcessor.NativeForkAnalyze"

        private val handler = MatrixHandlerThread.getDefaultHandler()
    }

    private class RetryRepository(dir: File) {

        private val hprofDir by lazy {
            File(dir, "hprof").apply {
                if (!exists()) mkdirs()
            }
        }

        private val keyDir by lazy {
            File(dir, "key").apply {
                if (!exists()) mkdirs()
            }
        }

        fun save(hprof: File, activity: String, key: String): Boolean {
            try {
                if (!hprof.isFile) return false
                val id = UUID.randomUUID().toString()
                MatrixLog.i(TAG, "Save HPROF file ${hprof.name} with id ${id}.")
                File(hprofDir, id)
                    .also {
                        hprof.copyTo(it, true)
                    }
                File(keyDir, id)
                    .apply {
                        createNewFile()
                    }
                    .bufferedWriter()
                    .use {
                        it.write(activity)
                        it.newLine()
                        it.write(key)
                        it.flush()
                    }
                return true
            } catch (throwable: Throwable) {
                MatrixLog.printErrStackTrace(
                    TAG, throwable,
                    "Failed to save HPROF record into retry repository"
                )
                return false
            }
        }

        fun process(action: (File, String, String) -> Unit) {
            val hprofs = hprofDir.listFiles() ?: emptyArray<File>()
            hprofs.forEach { hprofFile ->
                val keyFile = File(keyDir, hprofFile.name)
                try {
                    if (keyFile.isFile) {
                        val (activity, key) = keyFile.bufferedReader().use {
                            Pair(it.readLine(), it.readLine())
                        }
                        action.invoke(hprofFile, activity, key)
                    }
                } catch (throwable: Throwable) {
                    MatrixLog.printErrStackTrace(
                        TAG, throwable,
                        "Failed to read HPROF record in retry repository"
                    )
                } finally {
                    hprofFile.deleteIfExist()
                    keyFile.deleteIfExist()
                }
            }
        }
    }

    private val retryRepo: RetryRepository? by lazy {
        try {
            File(watcher.context.cacheDir, "matrix_res_canary_process_retry")
                .apply {
                    if (!isDirectory) {
                        deleteIfExist()
                        mkdirs()
                    }
                }
                .let {
                    RetryRepository(it)
                }
        } catch (throwable: Throwable) {
            MatrixLog.printErrStackTrace(TAG, throwable, "")
            null
        }
    }

    private val screenStateReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            MatrixLog.i(TAG, "Screen is off.")
            handler.post {
                MatrixLog.i(TAG, "Start iterating files.")
                retryRepo?.process { hprof, activity, key ->
                    MatrixLog.i(TAG, "Found record ${hprof.name} (activity: ${activity}, key: ${key}).")
                    val result =
                        MemoryUtil.analyze(hprof.absolutePath, key, timeout = 1200)
                    if (result.mLeakFound) {
                        publishIssue(
                            SharePluginInfo.IssueType.LEAK_FOUND,
                            ResourceConfig.DumpMode.FORK_ANALYSE,
                            activity, key, result.toString(),
                            result.mAnalysisDurationMs.toString(), 1
                        )
                    } else if (result.mFailure != null) {
                        publishIssue(
                            SharePluginInfo.IssueType.ERR_EXCEPTION,
                            ResourceConfig.DumpMode.FORK_ANALYSE,
                            activity, key, result.mFailure.toString(),
                            result.mAnalysisDurationMs.toString(), 1
                        )
                    }
                }
            }
        }
    }

    init {
        try {
            IntentFilter().apply {
                addAction(Intent.ACTION_SCREEN_OFF)
            }.let {
                watcher.resourcePlugin.application.registerReceiver(screenStateReceiver, it)
                MatrixLog.i(TAG, "Filter registered.")
            }
        } catch (throwable: Throwable) {
            MatrixLog.printErrStackTrace(TAG, throwable, "")
        }
    }

    override fun process(destroyedActivityInfo: DestroyedActivityInfo): Boolean {

        val hprof = dumpStorageManager.newHprofFile() ?: run {
            publishIssue(
                SharePluginInfo.IssueType.LEAK_FOUND,
                ResourceConfig.DumpMode.FORK_ANALYSE,
                "[unknown]", "[unknown]", "Failed to create hprof file.", "0"
            )
            return true
        }

        val activity = destroyedActivityInfo.mActivityName
        val key = destroyedActivityInfo.mKey

        val result = MemoryUtil.dumpAndAnalyze(hprof.absolutePath, key, timeout = 600)
        if (result.mFailure != null) {
            // Copies file to retry repository and analyzes it again when the screen is locked.
            MatrixLog.i(TAG, "Process failed, move into retry repository.")
            if (retryRepo?.save(hprof, activity, key) != true) {
                publishIssue(
                    SharePluginInfo.IssueType.ERR_EXCEPTION,
                    ResourceConfig.DumpMode.FORK_ANALYSE,
                    activity, key, result.mFailure.toString(),
                    result.mAnalysisDurationMs.toString()
                )
            }
        } else {
            if (result.mLeakFound) {
                publishIssue(
                    SharePluginInfo.IssueType.LEAK_FOUND,
                    ResourceConfig.DumpMode.FORK_ANALYSE,
                    activity, key, result.toString(), result.mAnalysisDurationMs.toString()
                )
            }
        }

        hprof.deleteIfExist()

        return true
    }
}

