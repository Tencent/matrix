package com.tencent.matrix.resource

import android.os.Debug
import com.tencent.matrix.resource.analyzer.model.ActivityLeakResult
import com.tencent.matrix.resource.analyzer.model.ReferenceChain
import com.tencent.matrix.resource.analyzer.model.ReferenceTraceElement
import com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.Future
import java.util.concurrent.FutureTask

/**
 * Memory non-suspending dump / analyze util.
 *
 * The util is used to dump / analyze heap memory into a HPROF file, like [Debug.dumpHprofData] and
 * LeakCanary. However, all API of the util can dump / analyze memory without suspending runtime.
 * The util will fork a new process for dumping / analyzing
 * (See [Copy-on-Write](https://en.wikipedia.org/wiki/Copy-on-write)).
 *
 * The idea is from [KOOM](https://github.com/KwaiAppTeam/KOOM).
 *
 * @author aurorani
 * @since 2022/02/09
 */
object MemoryUtil {

    private const val TAG = "Matrix.MemoryUtil"

    private fun info(message: String) {
        MatrixLog.i(TAG, message)
    }

    private fun error(message: String, throwable: Throwable? = null) {
        if (throwable != null)
            MatrixLog.printErrStackTrace(TAG, throwable, message)
        else
            MatrixLog.e(TAG, message)
    }

    private const val DEFAULT_TASK_TIMEOUT = 0L

    private val initialized by lazy {
        System.loadLibrary("matrix_resource_canary")
        initialize().also {
            if (!it)
                error("Failed to initialize resources.", RuntimeException())
        }
    }

    private external fun initialize(): Boolean

    private inline fun <T> initSafe(failed: T, action: () -> T): T {
        if (initialized) return action.invoke()
        error("Skip by reason of failure initialization.")
        return failed
    }

    /**
     * Dumps HPROF to specific file on [hprofPath]. The function will suspend current thread.
     *
     * The task will be cancelled in [timeout] seconds. If [timeout] is zero, the task will be
     * executed without time limitation.
     *
     * The function may cause jank and garbage collection, and will not throw exceptions but return
     * false and print stack trace if error happened.
     */
    @JvmStatic
    @JvmOverloads
    fun dumpBlock(
        hprofPath: String,
        timeout: Long = DEFAULT_TASK_TIMEOUT
    ): Boolean = initSafe(false) {
        return when (val pid = fork(timeout)) {
            -1 -> run {
                error("Failed to fork task executing process.")
                false
            }
            0 -> run { // task process
                // Unnecessary to catch exception because new exception instance cannot be created
                // in forked process. The process just exits with non-zero code.
                Debug.dumpHprofData(hprofPath)
                exit(0)
                true
            }
            else -> run { // current process
                info("Start dumping HPROF. Wait for task process [${pid}] complete executing.")
                val code = wait(pid)
                info("Complete dumping HPROF with code ${code}.")
                code == 0
            }
        }
    }

    /**
     * Dumps HPROF to specific file on [hprofPath]. The function will dump memory as an asynchronous
     * computation.
     *
     * The task will be cancelled in [timeout] seconds. If [timeout] is zero, the task will be
     * executed without time limitation.
     *
     * The function may cause jank and garbage collection, and will not throw exceptions but return
     * false and print stack trace if error happened.
     */
    @JvmStatic
    @JvmOverloads
    fun dumpAsync(
        hprofPath: String,
        timeout: Long = DEFAULT_TASK_TIMEOUT,
        callback: (Boolean) -> Unit
    ) {
        MatrixHandlerThread.getDefaultHandler().post {
            callback.invoke(dumpBlock(hprofPath, timeout))
        }
    }

    /**
     * Dumps HPROF to specific file on [hprofPath]. The function will dump memory as an asynchronous
     * computation.
     *
     * The task will be cancelled in [timeout] seconds. If [timeout] is zero, the task will be
     * executed without time limitation.
     *
     * The function may cause jank and garbage collection, and will not throw exceptions but return
     * false and print stack trace if error happened.
     */
    @JvmStatic
    @JvmOverloads
    fun dumpAsync(
        hprofPath: String,
        timeout: Long = DEFAULT_TASK_TIMEOUT
    ): Future<Boolean> {
        return FutureTask<Boolean> {
            dumpBlock(hprofPath, timeout)
        }
    }

    private fun createAnalyzeFile(): File =
        File.createTempFile("matrix_mem_analyze-", null)

    private external fun analyze(
        hprofPath: String,
        resultPath: String,
        referenceKey: String
    ): Boolean

    private fun convertReferenceType(value: Int): ReferenceTraceElement.Type =
        when (value) {
            // It is an end-tag that can only be the last chain element reference type.
            0 -> ReferenceTraceElement.Type.INSTANCE_FIELD
            1 -> ReferenceTraceElement.Type.STATIC_FIELD
            2 -> ReferenceTraceElement.Type.INSTANCE_FIELD
            3 -> ReferenceTraceElement.Type.ARRAY_ENTRY
            else -> throw IllegalArgumentException("Unsupported reference type $value")
        }

    private fun convertObjectType(value: Int): ReferenceTraceElement.Holder =
        when (value) {
            1 -> ReferenceTraceElement.Holder.CLASS
            2 -> ReferenceTraceElement.Holder.ARRAY
            3 -> ReferenceTraceElement.Holder.OBJECT
            else -> throw IllegalArgumentException("Unsupported object type $value")
        }

    private data class LeakChain(val nodes: List<Node>) {
        data class Node(
            val objectName: String,
            val objectType: Int,
            val referenceName: String,
            val referenceType: Int
        )

        fun convertToReferenceChain(): ReferenceChain =
            nodes.map {
                ReferenceTraceElement(
                    it.referenceName,
                    convertReferenceType(it.referenceType),
                    convertObjectType(it.objectType),
                    it.objectName,
                    null, null, emptyList()
                )
            }.let {
                ReferenceChain(it)
            }
    }

    private fun deserialize(file: File): List<LeakChain> {
        val stream = file.inputStream()
        val chainCount = stream.readOrderedInt()
        if (chainCount == 0) {
            stream.close()
            return emptyList()
        }
        val result = mutableListOf<LeakChain>()
        for (chainIndex in 0 until chainCount) {
            val nodes = mutableListOf<LeakChain.Node>()
            val chainLength = stream.readOrderedInt()
            for (nodeIndex in 0 until chainLength) {
                // node
                val objectType = stream.readOrderedInt()
                val objectName = stream.readString(stream.readOrderedInt())

                // reference
                val referenceType = stream.readOrderedInt()
                val referenceName =
                    if (referenceType == 0) ""  // reached end tag
                    else stream.readString(stream.readOrderedInt())

                nodes += LeakChain.Node(objectName, objectType, referenceName, referenceType)
            }
            result += LeakChain(nodes)
        }
        stream.close()
        return result
    }

    /**
     * Deserializes analyze result file generated from analyze functions.
     */
    @JvmStatic
    @Throws(IOException::class)
    fun deserializeAnalyzeResult(file: File): List<ReferenceChain> =
        deserialize(file).map { it.convertToReferenceChain() }

    /**
     * Dumps HPROF to specific file on [hprofPath] and analyzes it immediately. The function will
     * suspend current thread.
     *
     * Object referring from [DestroyedActivityInfo] instance with key [referenceKey] will be marked
     * as leak object while analyzing.
     *
     * The task will be cancelled in [timeout] seconds. If [timeout] is zero, the task will be
     * executed without time limitation.
     *
     * The function creates a binary file containing analyze result, the result file will be saved
     * if [keepResult] is true, and it can be deserialized by function [deserializeAnalyzeResult].
     *
     * The function may cause jank and garbage collection, and will not throw exceptions but return
     * false and print stack trace if error happened.
     */
    @JvmStatic
    @JvmOverloads
    fun analyzeBlock(
        hprofPath: String,
        referenceKey: String,
        timeout: Long = DEFAULT_TASK_TIMEOUT,
        keepResult: Boolean = false
    ): ActivityLeakResult =
        initSafe(ActivityLeakResult.failure(RuntimeException("Initialize failed."), 0)) {
            val analyzeStart = System.currentTimeMillis()
            val resultFile = try {
                createAnalyzeFile()
            } catch (exception: IOException) {
                return ActivityLeakResult.failure(
                    RuntimeException("Failed to create temporary analyze file", exception),
                    0
                )
            }
            val resultPath = resultFile.absolutePath
            return when (val pid = fork(timeout)) {
                -1 -> run {
                    ActivityLeakResult.failure(
                        RuntimeException("Failed to fork task executing process."),
                        0
                    )
                }
                0 -> run { // task process
                    // Unnecessary to catch exception because new exception instance cannot be created
                    // in forked process. The process just exits with non-zero code.
                    Debug.dumpHprofData(hprofPath)
                    val code = if (analyze(hprofPath, resultPath, referenceKey)) 0 else -1
                    exit(code)
                    ActivityLeakResult.noLeak(0) // Unreachable.
                }
                else -> run { // current process
                    info("Start analyzing memory. Wait for task process [${pid}] complete executing.")
                    val code = wait(pid)
                    info("Complete analyzing memory with code ${code}.")
                    return try {
                        val chains = deserialize(resultFile)
                        if (chains.isEmpty()) {
                            ActivityLeakResult.noLeak(System.currentTimeMillis() - analyzeStart)
                        } else {
                            // TODO: support reporting multiple leak chain.
                            chains.first().run {
                                ActivityLeakResult.leakDetected(
                                    false,
                                    nodes.last().objectName,
                                    convertToReferenceChain(),
                                    System.currentTimeMillis() - analyzeStart
                                )
                            }
                        }
                    } catch (exception: Exception) {
                        ActivityLeakResult.failure(
                            exception,
                            System.currentTimeMillis() - analyzeStart
                        )
                    } finally {
                        if (!keepResult) {
                            if (resultFile.exists()) resultFile.delete()
                        }
                    }
                }
            }
        }

    /**
     * Dumps HPROF to specific file on [hprofPath] and analyzes it immediately. The function will
     * dump memory as an asynchronous computation.
     *
     * Object referring from [DestroyedActivityInfo] instance with key [referenceKey] will be marked
     * as leak object while analyzing.
     *
     * The task will be cancelled in [timeout] seconds. If [timeout] is zero, the task will be
     * executed without time limitation.
     *
     * The function creates a binary file containing analyze result, the result file will be saved
     * if [keepResult] is true, and it can be deserialized by function [deserializeAnalyzeResult].
     *
     * The function may cause jank and garbage collection, and will not throw exceptions but return
     * false and print stack trace if error happened.
     */
    @JvmStatic
    @JvmOverloads
    fun analyzeAsync(
        hprofPath: String,
        referenceKey: String,
        timeout: Long = DEFAULT_TASK_TIMEOUT,
        keepResult: Boolean = false
    ): Future<ActivityLeakResult> =
        FutureTask<ActivityLeakResult> {
            analyzeBlock(hprofPath, referenceKey, timeout, keepResult)
        }
}

/**
 * Fork dump process. To solve the deadlock, this function will suspend the symbol.runtime and resume it in
 * parent process.
 *
 * The function is implemented with native standard fork().
 * See [man fork](https://man7.org/linux/man-pages/man2/fork.2.html).
 */
private external fun fork(timeout: Long): Int

/**
 * Wait dump process exits and return exit status of child process.
 *
 * The function is implemented with native standard waitpid().
 * See [man wait](https://man7.org/linux/man-pages/man2/wait.2.html).
 */
private external fun wait(pid: Int): Int

/**
 * Exit current process.
 *
 * The function is implemented with native standard _exit().
 * See [man _exit](https://man7.org/linux/man-pages/man2/exit.2.html).
 */
private external fun exit(code: Int)

private fun InputStream.readOrderedInt(): Int {
    val buffer = ByteBuffer.allocate(4)
        .apply {
            order(ByteOrder.nativeOrder())
        }
    read(buffer.array(), 0, 4)
    return buffer.getInt(0)
}

private fun InputStream.readString(length: Int): String {
    val buffer = ByteArray(length)
    read(buffer)
    return String(buffer, Charsets.UTF_8)
}