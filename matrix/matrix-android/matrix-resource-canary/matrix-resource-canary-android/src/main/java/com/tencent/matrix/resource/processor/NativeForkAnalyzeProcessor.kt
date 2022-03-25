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
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil
import java.io.File
import java.util.*
import java.util.concurrent.Executors

private fun File.deleteIfExist() {
    if (exists()) delete()
}

private const val TAG = "Matrix.LeakProcessor.NativeForkAnalyze"

private class RetryRepository(private val dir: File) {

    private val hprofDir
        get() = File(dir, "hprof").apply {
            if (!exists()) mkdirs()
        }

    private val keyDir
        get() = File(dir, "key").apply {
            if (!exists()) mkdirs()
        }

    /**
     * The lock used to avoid processing uncopied files.
     */
    private val accessLock = Any()

    fun save(hprof: File, activity: String, key: String, failure: String): Boolean {
        try {
            if (!hprof.isFile) return false
            val id = UUID.randomUUID().toString()
            MatrixLog.i(TAG, "Save HPROF analyze retry record ${activity}(${id}).")
            synchronized(accessLock) {
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
                        it.newLine()
                        it.write(failure)
                        it.flush()
                    }
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

    fun process(action: (File, String, String, String) -> Unit) {
        val hprofs = synchronized(accessLock) {
            hprofDir.listFiles() ?: emptyArray<File>()
        }
        hprofs.forEach { hprofFile ->
            val keyFile = File(keyDir, hprofFile.name)
            try {
                if (keyFile.isFile) {
                    val (activity, key, failure) = keyFile.bufferedReader().use {
                        Triple(it.readLine(), it.readLine(), it.readLine())
                    }
                    action.invoke(hprofFile, activity, key, failure)
                }
            } catch (throwable: Throwable) {
                MatrixLog.printErrStackTrace(
                    TAG, throwable,
                    "Failed to read HPROF record from retry repository"
                )
            } finally {
                hprofFile.deleteIfExist()
                keyFile.deleteIfExist()
            }
        }
    }
}

class NativeForkAnalyzeProcessor(watcher: ActivityRefWatcher) : BaseLeakProcessor(watcher) {

    companion object {

        private const val RETRY_THREAD_NAME = "matrix_res_native_analyze_retry"

        private const val RETRY_REPO_NAME = "matrix_res_process_retry"

        private val retryExecutor by lazy {
            Executors.newSingleThreadExecutor {
                Thread(it, RETRY_THREAD_NAME)
            }
        }
    }

    private val retryRepo: RetryRepository? by lazy {
        try {
            File(watcher.context.cacheDir, RETRY_REPO_NAME)
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
            MatrixLog.printErrStackTrace(TAG, throwable, "Failed to initialize retry repository")
            null
        }
    }

    private val screenStateReceiver by lazy {
        return@lazy object : BroadcastReceiver() {
            override fun onReceive(context: Context?, intent: Intent?) {
                retryExecutor.execute {
                    retryRepo?.process { hprof, activity, key, failure ->
                        MatrixLog.i(TAG, "Found record ${activity}(${hprof.name}).")
                        val historyFailure = mutableListOf<String>().apply {
                            add(failure)
                        }
                        var retryCount = 1
                        var result = MemoryUtil.analyze(hprof.absolutePath, key, timeout = 1200)
                        if (result.mFailure != null) {
                            historyFailure.add(result.mFailure.toString())
                            retryCount++
                            result = analyze(hprof, key)
                        }
                        if (result.mLeakFound) {
                            watcher.markPublished(activity, false)
                            publishIssue(
                                SharePluginInfo.IssueType.LEAK_FOUND,
                                ResourceConfig.DumpMode.FORK_ANALYSE,
                                activity, key, result.toString(),
                                result.mAnalysisDurationMs.toString(), retryCount
                            )
                        } else if (result.mFailure != null) {
                            historyFailure.add(result.mFailure.toString())
                            publishIssue(
                                SharePluginInfo.IssueType.ERR_EXCEPTION,
                                ResourceConfig.DumpMode.FORK_ANALYSE,
                                activity, key, historyFailure.joinToString(";"),
                                result.mAnalysisDurationMs.toString(), retryCount
                            )
                        }
                    }
                }
            }
        }
    }

    init {
        if (MatrixUtil.isInMainProcess(watcher.context)) {
            try {
                IntentFilter().apply {
                    addAction(Intent.ACTION_SCREEN_OFF)
                }.let {
                    screenStateReceiver.also { receiver ->
                        MatrixLog.i(TAG, "Screen state receiver $receiver registered.")
                        watcher.resourcePlugin.application.registerReceiver(receiver, it)
                    }
                }
            } catch (throwable: Throwable) {
                MatrixLog.printErrStackTrace(TAG, throwable, "")
            }
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
            val failure = result.mFailure.toString()
            if (retryRepo?.save(hprof, activity, key, failure) != true) {
                publishIssue(
                    SharePluginInfo.IssueType.ERR_EXCEPTION,
                    ResourceConfig.DumpMode.FORK_ANALYSE,
                    activity, key, failure,
                    result.mAnalysisDurationMs.toString()
                )
            }
        } else {
            if (result.mLeakFound) {
                watcher.markPublished(activity, false)
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

