package com.tencent.matrix.resource

import android.os.Debug
import com.tencent.matrix.Matrix
import com.tencent.matrix.resource.analyzer.model.ActivityLeakResult
import com.tencent.matrix.resource.analyzer.model.ReferenceChain
import com.tencent.matrix.resource.analyzer.model.ReferenceTraceElement
import com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo
import com.tencent.matrix.util.MatrixLog
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.text.SimpleDateFormat
import java.util.*

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

/**
 * Memory non-suspending dump / analyze util.
 *
 * The util is used to dump / analyze heap memory into a HPROF file, like [Debug.dumpHprofData] and
 * LeakCanary. However, all dump API of the util can dump memory without suspending runtime because
 * the util will fork a new process for dumping
 * (See [Copy-on-Write](https://en.wikipedia.org/wiki/Copy-on-write)).
 *
 * The idea is from [KOOM](https://github.com/KwaiAppTeam/KOOM).
 *
 * @author aurorani
 * @since 2022/02/09
 */
object MemoryUtil {

    private const val DEFAULT_TASK_TIMEOUT = 0L

    private val storageDir =
        File(Matrix.with().application.cacheDir, "matrix_mem_util").apply {
            assureIsDirectory()
        }

    private val initialized by lazy {
        System.loadLibrary("matrix_resource_canary")
        if (!loadJniCache()) {
            error("Failed to load JNI cache.")
            return@lazy false
        }
        if (!initializeTaskStateDir()) {
            error("Failed to initialize task state directory.")
            return@lazy false
        }
        if (!initializeSymbol()) {
            error("Failed to initialize resources.")
            return@lazy false
        }
        return@lazy true
    }

    private fun initializeTaskStateDir(): Boolean {
        val taskStateDir = File(storageDir, "/ts").apply {
            if (exists()) delete()
            mkdirs()
        }
        return syncTaskStateDir(taskStateDir.absolutePath)
    }

    private external fun syncTaskStateDir(path: String): Boolean

    private external fun initializeSymbol(): Boolean

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
    fun dump(
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
                updateTaskState(TaskState.DUMP)
                Debug.dumpHprofData(hprofPath)
                exit(0)
                true
            }
            else -> run { // current process
                info("Start dumping HPROF. Wait for task process [${pid}] complete executing.")
                val result = wait(pid)
                info("Complete dumping HPROF with error: ${result.errorMessage}.")
                result.success
            }
        }
    }

    private external fun analyzeInternal(
        hprofPath: String,
        resultPath: String,
        referenceKey: String
    ): Boolean

    private val initializeFailedResult
        get() = ActivityLeakResult.failure(RuntimeException("Initialize failed."), 0)

    /**
     * Analyze existing HPROF file [hprofPath]. This function will suspend current thread.
     *
     * Object referring from [DestroyedActivityInfo] instance with key [referenceKey] will be marked
     * as leak object while analyzing.
     *
     * Unlike other APIs in [MemoryUtil], this function will not fork a new process but execute task
     * on current process. Memory usage of current process will increase while analyzing.
     *
     * The function creates a binary file containing analyze result, the result file will be saved
     * if [keepResult] is true, and it can be deserialized by function [deserializeAnalyzeResult].
     *
     * The function may cause jank and garbage collection, and will not throw exceptions but return
     * a failure result if error happened, see [ActivityLeakResult.failure].
     */
    @JvmStatic
    @JvmOverloads
    fun analyze(
        hprofPath: String,
        referenceKey: String,
        keepResult: Boolean = false
    ): ActivityLeakResult = initSafe(initializeFailedResult) {
        val analyzeStart = System.currentTimeMillis()
        val resultFile = createAnalyzeFile()
            ?: return ActivityLeakResult.failure(
                RuntimeException("Failed to create temporary analyze result file"), 0
            )
        val resultPath = resultFile.absolutePath
        return try {
            if (analyzeInternal(hprofPath, resultPath, referenceKey)) {
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
            } else {
                ActivityLeakResult.failure(
                    RuntimeException("Analyze failed."),
                    System.currentTimeMillis() - analyzeStart
                )
            }
        } catch (exception: Exception) {
            ActivityLeakResult.failure(exception, System.currentTimeMillis() - analyzeStart)
        } finally {
            if (!keepResult) {
                if (resultFile.exists()) resultFile.delete()
            }
        }
    }

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
     * a failure result if error happened, see [ActivityLeakResult.failure].
     */
    @JvmStatic
    @JvmOverloads
    fun dumpAndAnalyze(
        hprofPath: String,
        referenceKey: String,
        timeout: Long = DEFAULT_TASK_TIMEOUT,
        keepResult: Boolean = false
    ): ActivityLeakResult =
        initSafe(initializeFailedResult) {
            val analyzeStart = System.currentTimeMillis()
            val resultFile = createAnalyzeFile()
                ?: return ActivityLeakResult.failure(
                    RuntimeException("Failed to create temporary analyze result file"), 0
                )
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
                    updateTaskState(TaskState.DUMP)
                    Debug.dumpHprofData(hprofPath)
                    val code = if (analyzeInternal(hprofPath, resultPath, referenceKey)) 0 else -1
                    exit(code)
                    ActivityLeakResult.noLeak(0) // Unreachable.
                }
                else -> run { // current process
                    info("Start analyzing memory. Wait for task process [${pid}] complete executing.")
                    val result = wait(pid)
                    info("Complete analyzing memory with error: ${result.errorMessage}.")
                    if (!result.success) {
                        return ActivityLeakResult.failure(
                            RuntimeException("Analyze failed with error message: ${result.errorMessage}. Last task state: ${result.lastState}."),
                            System.currentTimeMillis() - analyzeStart
                        )
                    }
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
                            exception, System.currentTimeMillis() - analyzeStart
                        )
                    } finally {
                        if (!keepResult) {
                            if (resultFile.exists()) resultFile.delete()
                        }
                    }
                }
            }
        }

    private val analyzeResultDir by lazy {
        File(storageDir, "analyze").apply {
            assureIsDirectory()
        }
    }

    private val analyzeFileTimeFormat = SimpleDateFormat("yyyy-MM-dd-HH-mm-ss", Locale.US)

    private fun createAnalyzeFile(): File? {
        val time = analyzeFileTimeFormat.format(Calendar.getInstance().time)
        val result = File(analyzeResultDir, "analyze-${time}.tmp")
        return try {
            result.apply {
                createNewFile()
            }
        } catch (exception: IOException) {
            error("Failed to create analyze result file on path ${result.absolutePath}.")
            null
        }
    }

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

    /**
     * Deserialize the analyze result file.
     *
     * The basic fields of the binary output are number (unsigned 32-bit integer) and string
     * (NOT null-terminated).
     *
     * The format of analyze result file look like:
     *
     * ```
     * number byte_order_magic
     * number count_of_leak_chains
     * leak_chain[count_of_leak_chains] leak_chains
     * ```
     *
     * Which with sub-records look like:
     *
     * ```
     * leak_chain:
     * number chain_length
     * chain_node[chain_length] chain_nodes
     *
     * chain_node:
     * node node
     * reference/end_tag reference
     *
     * node:
     * number node_type
     * number node_name_length
     * string node_name
     *
     * reference:
     * number reference_type
     * number reference_name_length
     * string reference_name
     *
     * end_tag:
     * number always_zero_end_tag
     * ```
     */
    private fun deserialize(file: File): List<LeakChain> {
        val stream = run {
            val input = file.inputStream()
            val magic = ByteArray(4)
            input.read(magic, 0, 4)
            if (magic.contentEquals(byteArrayOf(0x00, 0x00, 0x00, 0x01)))
                OrderedStreamWrapper(ByteOrder.BIG_ENDIAN, input)
            else
                OrderedStreamWrapper(ByteOrder.LITTLE_ENDIAN, input)
        }
        try {
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
            return result
        } catch (exception: IOException) {
            throw exception
        } finally {
            stream.close()
        }
    }

    /**
     * Deserializes analyze result file generated from analyze functions.
     */
    @JvmStatic
    @Throws(IOException::class)
    fun deserializeAnalyzeResult(file: File): List<ReferenceChain> =
        deserialize(file).map { it.convertToReferenceChain() }
}

private external fun loadJniCache(): Boolean

private object TaskState {
    const val DUMP: Byte = 1
    const val ANALYZER_CREATE: Byte = 2
    const val ANALYZER_INITIALIZE: Byte = 3
    const val ANALYZER_EXECUTE: Byte = 4
    const val SERIALIZE_RESULT: Byte = 5
}

/**
 * Update task state in child process.
 *
 * The function is designed as a native function because the runtime is broken that we cannot
 * allocate any object.
 */
private external fun updateTaskState(state: Byte)

/**
 * Fork dump process. To solve the deadlock, this function will suspend the symbol.runtime and resume it in
 * parent process.
 *
 * The function is implemented with native standard fork().
 * See [man fork](https://man7.org/linux/man-pages/man2/fork.2.html).
 */
private external fun fork(timeout: Long): Int

private class TaskResult(
    private val type: Int,
    private val code: Int,
    private val last: Byte
) {
    companion object {
        private const val TYPE_WAIT_FAILED = -1
        private const val TYPE_EXIT = 0
        private const val TYPE_SIGNALED = 1
    }

    val success: Boolean
        get() = type == TYPE_EXIT && code == 0

    val errorMessage: String?
        get() {
            if (success) return null
            return when (type) {
                TYPE_WAIT_FAILED -> "failed to invoke waitpid() (errno: $code)"
                TYPE_EXIT -> "task process exit with status $code"
                TYPE_SIGNALED -> "task process was terminated by signal $code"
                else -> "unknown error"
            }
        }

    val lastState: String
        get() {
            return when (last) {
                TaskState.DUMP -> "dump"
                TaskState.ANALYZER_CREATE -> "analyzer_create"
                TaskState.ANALYZER_INITIALIZE -> "analyzer_initialize"
                TaskState.ANALYZER_EXECUTE -> "analyzer_execute"
                TaskState.SERIALIZE_RESULT -> "serialize_result"
                else -> "unknown"
            }
        }
}

/**
 * Wait dump process exits and return exit status of child process.
 *
 * The function is implemented with native standard waitpid().
 * See [man wait](https://man7.org/linux/man-pages/man2/wait.2.html).
 */
private external fun wait(pid: Int): TaskResult

/**
 * Exit current process.
 *
 * The function is implemented with native standard _exit().
 * See [man _exit](https://man7.org/linux/man-pages/man2/exit.2.html).
 */
private external fun exit(code: Int)

private fun File.assureIsDirectory() {
    if (!isDirectory) {
        if (exists())
            throw IllegalStateException("Path $absolutePath is pointed to an existing element but it is not a directory.")
        mkdirs()
    }
}

private class OrderedStreamWrapper(
    private val order: ByteOrder,
    private val stream: InputStream
) {

    fun readOrderedInt(): Int {
        val buffer = ByteBuffer.allocate(4)
            .apply {
                order(order)
            }
        stream.read(buffer.array(), 0, 4)
        return buffer.getInt(0)
    }

    fun readString(length: Int): String {
        val buffer = ByteArray(length)
        stream.read(buffer)
        return String(buffer, Charsets.UTF_8)
    }

    fun close() {
        stream.close()
    }
}