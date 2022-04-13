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

private val currentTime: Long
    get() = System.currentTimeMillis()

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

/**
 * Memory non-suspending dump / analyze util.
 *
 * The util is used to dump / analyze heap memory into a HPROF file, like [Debug.dumpHprofData] and
 * LeakCanary. However, all APIs of the util can dump memory without suspending runtime because the
 * util will fork a new process for dumping (See
 * [Copy-on-Write](https://en.wikipedia.org/wiki/Copy-on-write)).
 *
 * The idea is from [KOOM](https://github.com/KwaiAppTeam/KOOM).
 *
 * @author aurorani
 * @since 2022/02/09
 */
object MemoryUtil {

    private const val DEFAULT_TASK_TIMEOUT = 0L

    private val storageDir: File =
        File(Matrix.with().application.cacheDir, "matrix_mem_util").apply {
            assureIsDirectory()
        }

    private external fun loadJniCache()

    private external fun syncTaskDir(storageDirPath: String)

    private external fun initializeSymbol()

    private val initialized: InitializeException? = run {
        try {
            System.loadLibrary("matrix_mem_util")
            loadJniCache()
            syncTaskDir(storageDir.absolutePath)
            initializeSymbol()
            return@run null
        } catch (throwable: Throwable) {
            return@run InitializeException(throwable)
        }
    }

    private inline fun <T> initSafe(action: (exception: InitializeException?) -> T): T {
        return action.invoke(initialized)
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
    ): Boolean = initSafe { exception ->
        if (exception != null) {
            error("", exception)
            return@initSafe false
        }
        return when (val pid = forkDump(hprofPath, timeout)) {
            -1 -> run {
                error("Failed to fork task executing process.")
                false
            }
            else -> run { // current process
                info("Wait for task process [${pid}] complete executing.")
                val result = waitTask(pid)
                result.exception?.let {
                    info("Task process [${pid}] complete with error: ${it.message}.")
                } ?: info("Task process [${pid}] complete without error.")
                return result.exception == null
            }
        }
    }

    private external fun forkDump(hprofPath: String, timeout: Long): Int

    private fun createTask(
        hprofPath: String,
        referenceKey: String,
        timeout: Long,
        forkTask: (String, String, String, Long) -> Int
    ): ActivityLeakResult = initSafe { exception ->
        if (exception != null) return@initSafe ActivityLeakResult.failure(exception, 0)
        val analyzeStart = currentTime
        val resultFile = createResultFile()
            ?: return ActivityLeakResult.failure(
                RuntimeException("Failed to create temporary analyze result file."), 0
            )
        val resultPath = resultFile.absolutePath
        val leakResult = when (val pid = forkTask(hprofPath, resultPath, referenceKey, timeout)) {
            -1 -> ActivityLeakResult.failure(ForkException(), 0)
            else -> run { // current process
                info("Wait for task process [${pid}] complete executing.")
                val result = waitTask(pid)
                if (result.exception != null) {
                    info("Task process [${pid}] complete with error: ${result.exception!!.message}.")
                    return@run ActivityLeakResult.failure(
                        result.exception, currentTime - analyzeStart
                    )
                } else {
                    info("Task process [${pid}] complete without error.")
                }
                return@run try {
                    val chains = deserialize(resultFile)
                    if (chains.isEmpty()) {
                        ActivityLeakResult.noLeak(currentTime - analyzeStart)
                    } else {
                        // TODO: support reporting multiple leak chain.
                        chains.first().run {
                            ActivityLeakResult.leakDetected(
                                false,
                                nodes.last().objectName,
                                convertToReferenceChain(),
                                currentTime - analyzeStart
                            )
                        }
                    }
                } catch (throwable: Throwable) {
                    ActivityLeakResult.failure(throwable, currentTime - analyzeStart)
                }
            }
        }
        if (resultFile.exists()) resultFile.delete()
        return@initSafe leakResult
    }

    /**
     * Analyze existing HPROF file [hprofPath]. This function will suspend current thread.
     *
     * Object referring from [DestroyedActivityInfo] instance with key [referenceKey] will be marked
     * as leak object while analyzing.
     *
     * The function may cause jank and garbage collection, and will not throw exceptions but return
     * a failure result if error happened, see [ActivityLeakResult.failure].
     */
    @JvmStatic
    @JvmOverloads
    fun analyze(
        hprofPath: String,
        referenceKey: String,
        timeout: Long = DEFAULT_TASK_TIMEOUT,
    ): ActivityLeakResult =
        createTask(hprofPath, referenceKey, timeout, ::forkAnalyze)

    private external fun forkAnalyze(
        hprofPath: String, resultPath: String, referenceKey: String,
        timeout: Long
    ): Int

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
     * The function may cause jank and garbage collection, and will not throw exceptions but return
     * a failure result if error happened, see [ActivityLeakResult.failure].
     */
    @JvmStatic
    @JvmOverloads
    fun dumpAndAnalyze(
        hprofPath: String,
        referenceKey: String,
        timeout: Long = DEFAULT_TASK_TIMEOUT,
    ): ActivityLeakResult =
        createTask(hprofPath, referenceKey, timeout, ::forkDumpAndAnalyze)

    private external fun forkDumpAndAnalyze(
        hprofPath: String, resultPath: String, referenceKey: String,
        timeout: Long
    ): Int

    private val resultDir: File
        get() = File(storageDir, "result").apply {
            assureIsDirectory()
        }

    private val analyzeFileTimeFormat =
        SimpleDateFormat("yyyy-MM-dd-HH-mm-ss", Locale.US)

    private fun createResultFile(): File? {
        val time = analyzeFileTimeFormat.format(Calendar.getInstance().time)
        val result = File(resultDir, "result-${time}.tmp")
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

    private object TaskState {
        const val DUMP: Byte = 1
        const val ANALYZER_CREATE: Byte = 2
        const val ANALYZER_INITIALIZE: Byte = 3
        const val ANALYZER_EXECUTE: Byte = 4
        const val CREATE_RESULT_FILE: Byte = 5
        const val SERIALIZE: Byte = 6
    }

    private class TaskResult(
        private val type: Int,
        private val code: Int,
        private val stateRaw: Byte,
        val error: String
    ) {
        companion object {
            private const val TYPE_WAIT_FAILED = -1
            private const val TYPE_EXIT = 0
            private const val TYPE_SIGNALED = 1
        }

        private val state: String
            get() {
                return when (stateRaw) {
                    TaskState.DUMP -> "dump"
                    TaskState.ANALYZER_CREATE -> "analyzer_create"
                    TaskState.ANALYZER_INITIALIZE -> "analyzer_initialize"
                    TaskState.ANALYZER_EXECUTE -> "analyzer_execute"
                    TaskState.CREATE_RESULT_FILE -> "create_result_file"
                    TaskState.SERIALIZE -> "serialize"
                    else -> "unknown"
                }
            }

        private val success: Boolean
            get() = type == TYPE_EXIT && code == 0

        val exception by lazy {
            if (success) return@lazy null
            return@lazy when (type) {
                TYPE_WAIT_FAILED -> WaitException(code)
                TYPE_EXIT -> UnexpectedExitException(code, state, error)
                TYPE_SIGNALED -> TerminateException(code, state, error)
                else -> UnknownAnalyzeException(type, code, state, error)
            }
        }
    }

    /**
     * Waits task process exits and returns exit status of child process.
     *
     * The function is implemented with native standard waitpid().
     * See [man wait](https://man7.org/linux/man-pages/man2/wait.2.html).
     */
    private external fun waitTask(pid: Int): TaskResult

    private class InitializeException(throwable: Throwable) :
        RuntimeException("Initialization failed due to: $throwable")

    private class ForkException :
        RuntimeException("Failed to fork task process")

    private class WaitException(errno: Int) :
        RuntimeException("Failed to wait task process with errno $errno")

    private class UnexpectedExitException(code: Int, state: String, error: String) :
        RuntimeException("Task process exited with code $code unexpectedly (state: ${state}, error: ${error})")

    private class TerminateException(signal: Int, state: String, error: String) :
        RuntimeException("Task process was terminated by signal $signal (state: ${state}, error: ${error})")

    private class UnknownAnalyzeException(type: Int, code: Int, state: String, error: String) :
        RuntimeException("Unknown error with type $type returned from task process (code: ${code}, state: ${state}, error: ${error})")
}