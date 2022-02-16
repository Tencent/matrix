package com.tencent.matrix.memorydump

import android.os.Debug
import com.tencent.matrix.resource.MemoryUtil
import com.tencent.matrix.util.MatrixHandlerThread
import java.util.concurrent.Future
import java.util.concurrent.FutureTask
import com.tencent.matrix.util.MatrixLog
import java.io.InputStream

private const val TAG = "Matrix.MemoryDump"

/**
 * Memory dump manager.
 *
 * The manager is used to dump heap memory into a HPROF file, like [Debug.dumpHprofData]. However,
 * there are several API of manager can dump memory without suspend runtime. The manager will fork
 * a new process for dumping (See [Copy-on-Write](https://en.wikipedia.org/wiki/Copy-on-write)).
 *
 * The idea is from [KOOM](https://github.com/KwaiAppTeam/KOOM).
 *
 * @author aurorani
 * @since 2021/10/20
 */
@Deprecated("Use MemoryUtil in matrix resource canary instead.")
object MemoryDumpManager {

    private const val DEFAULT_DUMP_TIMEOUT = 60L
    /**
     * Callback when dump process completes dumping memory.
     *
     * TODO: Convert to SAM interface after upgrading Kotlin version to 1.4.
     *
     * @author aurorani
     * @since 2021/10/25
     */
    interface DumpCallback {
        fun onDumpComplete(result: Boolean)
    }

    /**
     * Dump HPROF to specific file on [path]. It will suspend the whole runtime.
     *
     * The function may cause jank and garbage collection, and will not throw exceptions but return
     * false and print stack trace if error happened.
     */
    @JvmStatic
    @Deprecated("Use Debug.dumpHprofData() instead.")
    fun dumpSuspend(path: String): Boolean {
        return try {
            Debug.dumpHprofData(path)
            true
        } catch (exception: Exception) {
            MatrixLog.printErrStackTrace(TAG, exception, "")
            false
        }
    }

    /**
     * Dump HPROF to specific file on [path]. Compared to [dumpSuspend], it will only suspend current
     * thread.
     *
     * The function may cause jank and garbage collection, and will not throw exceptions but return
     * false and print stack trace if error happened.
     */
    @JvmStatic
    @JvmOverloads
    @Deprecated(
        "Use MemoryUtil.dump() instead.",
        ReplaceWith(
            "MemoryUtil.dump(path, timeout)",
            "com.tencent.matrix.resource.MemoryUtil"
        )
    )
    fun dumpBlock(path: String, timeout: Long = DEFAULT_DUMP_TIMEOUT): Boolean =
        MemoryUtil.dump(path, timeout)

    /**
     * Dump HPROF to specific file on [path]. It will dump memory as an asynchronous computation.
     *
     * The function may cause jank and garbage collection, and will not throw exceptions but return
     * false and print stack trace if error happened.
     */
    @JvmStatic
    @Deprecated(
        "Use MemoryUtil.dump() with custom task-scheduling instead.", ReplaceWith(
            "FutureTask { MemoryUtil.dump(path) }",
            "java.util.concurrent.FutureTask",
            "com.tencent.matrix.resource.MemoryUtil"
        )
    )
    fun dumpAsync(path: String): Future<Boolean> =
        FutureTask {
            MemoryUtil.dump(path)
        }

    /**
     * Dump HPROF to specific file on [path]. It will dump memory as an asynchronous computation.
     *
     * The function may cause jank and garbage collection, and will not throw exceptions but return
     * false and print stack trace if error happened.
     */
    @JvmStatic
    @Deprecated(
        "Use MemoryUtil.dump() with custom task-scheduling instead.",
        ReplaceWith(
            "Thread { callback.onDumpComplete(MemoryUtil.dump(path)) }.start()",
            "com.tencent.matrix.resource.MemoryUtil"
        ),
    )
    fun dumpAsync(path: String, callback: DumpCallback) =
        MatrixHandlerThread.getDefaultHandler().post {
            callback.onDumpComplete(MemoryUtil.dump(path))
        }
}

/**
 * Instance for handling remote memory dump.
 *
 * **Developer should invokes [InputStream.close] manually after dump process completes execution.**
 *
 * @property stream HPROF file input stream.
 * @property result [Future] instance to check dump execution is done or not and dump result.
 */
class DumpHandler internal constructor(
    val stream: InputStream,
    val result: Future<Boolean>
)