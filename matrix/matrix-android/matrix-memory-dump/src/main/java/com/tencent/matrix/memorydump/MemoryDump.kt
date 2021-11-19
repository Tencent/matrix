package com.tencent.matrix.memorydump

import android.os.Build
import android.os.Debug
import android.os.Process
import java.util.concurrent.Future
import java.util.concurrent.FutureTask
import com.tencent.matrix.util.MatrixLog
import java.io.BufferedInputStream
import java.io.FileInputStream
import java.io.InputStream
import java.util.concurrent.CompletableFuture
import java.util.concurrent.Executors

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
object MemoryDumpManager {

    private val initialized: Boolean = initialize()

    private const val DEFAULT_DUMP_TIMEOUT = 60L

    private val dumpExecutor = Executors.newSingleThreadExecutor {
        Thread(it, "matrix_memory_dump_executor")
    }

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
    fun dumpSuspend(path: String): Boolean {
        if (!initialized) {
            MatrixLog.e(TAG, "Memory dump manager is not successfully initialized. Skip dump.")
            return false
        }
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
    fun dumpBlock(path: String, timeout: Long = DEFAULT_DUMP_TIMEOUT): Boolean {
        if (!initialized) {
            MatrixLog.e(TAG, "Memory dump manager is not successfully initialized. Skip dump.")
            return false
        }
        MatrixLog.i(TAG, "[dump block, pid: ${Process.myPid()}] Fork dump process.")
        return when (val pid = fork(timeout)) {
            -1 -> run {
                MatrixLog.e(TAG, "Cannot fork child dump process.")
                false
            }
            0 -> run {
                try {
                    if (dumpSuspend(path)) exit(0) else exit(-1)
                } catch (exception: Exception) {
                    // The catch block may be unnecessary, because the runtime is broken and cannot
                        // create any exception to throw. But keep the code, whatever.
                    MatrixLog.printErrStackTrace(TAG, exception, "")
                    exit(-2)
                }
                true
            }
            else -> run {
                MatrixLog.i(
                    TAG,
                    "[dump block, pid: ${Process.myPid()}] Wait dump complete (dump process pid: ${pid})."
                )
                val result = wait(pid)
                MatrixLog.i(
                    TAG,
                    "[dump block, pid: ${Process.myPid()}] Dump complete, status code: $result."
                )
                result == 0
            }
        }
    }

    /**
     * Dump HPROF to specific file on [path]. It will dump memory as an asynchronous computation.
     *
     * The function may cause jank and garbage collection, and will not throw exceptions but return
     * false and print stack trace if error happened.
     */
    @JvmStatic
    fun dumpAsync(path: String): Future<Boolean> {
        if (!initialized) {
            MatrixLog.e(TAG, "Memory dump manager is not successfully initialized. Skip dump.")
            return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                CompletableFuture.completedFuture(false)
            } else {
                dumpExecutor.submit<Boolean> { false }
            }
        }
        return dumpExecutor.submit<Boolean> {
            dumpBlock(path)
        }
    }

    /**
     * Dump HPROF to specific file on [path]. It will dump memory as an asynchronous computation.
     *
     * The function may cause jank and garbage collection, and will not throw exceptions but return
     * false and print stack trace if error happened.
     */
    @JvmStatic
    fun dumpAsync(path: String, callback: DumpCallback) {
        if (!initialized) {
            MatrixLog.e(TAG, "Memory dump manager is not successfully initialized. Skip dump.")
            callback.onDumpComplete(false)
        }
        dumpExecutor.execute {
            callback.onDumpComplete(dumpBlock(path))
        }
    }

    /**
     * Dump HPROF as a buffered input stream. It will dump memory as an asynchronous computation.
     *
     * The function returns a dump handler including HPROF file input stream instance and a [Future]
     * object for listening dump process status, see [DumpHandler].
     *
     * The buffer size of buffered input stream will be set as default value of [BufferedInputStream]
     * if [bufferSize] is not positive.
     *
     * TODO: Extract function for two [dumpStream] using inline. DO NOT ALLOCATE ANY OBJECT, INCLUDING LAMBDA.
     *
     * The function may cause jank and garbage collection, and will not throw exceptions but return
     * false and print stack trace if error happened.
     */
    @JvmStatic
    @JvmOverloads
    fun dumpStream(
        bufferSize: Int = 0,
        timeout: Long = DEFAULT_DUMP_TIMEOUT
    ): DumpHandler? {
        if (!initialized) {
            MatrixLog.e(TAG, "Memory dump manager is not successfully initialized. Skip dump.")
            return null
        }
        val pointer = forkPipe(timeout)
        if (pointer == -1L) {
            MatrixLog.e(TAG, "Cannot fork child dump process.")
            return null
        }
        if (pointer == -2L) {
            MatrixLog.e(TAG, "Failed to allocate native resource in parent process.")
            return null
        }
        MatrixLog.i(TAG, "[dump stream, pid: ${Process.myPid()}] Fork dump process.")
        val pid = pidFromForkPair(pointer)
        val fd = fdFromForkPair(pointer)
        return when (pid) {
            0 -> run {
                try {
                    exit(dumpHprof(fd))
                } catch (exception: Exception) {
                    // The catch block may be unnecessary, because the runtime is broken and cannot
                        // create any exception to throw. But keep the code, whatever.
                    MatrixLog.printErrStackTrace(TAG, exception, "")
                    exit(-1)
                }
                throw RuntimeException("Unreachable code.")
            }
            else -> {
                free(pointer)
                val stream = FileInputStream("/proc/self/fd/${fd}").let {
                    if (bufferSize > 0) BufferedInputStream(it, bufferSize)
                    else BufferedInputStream(it)
                }
                val future = FutureTask<Boolean> {
                    MatrixLog.i(
                        TAG,
                        "[dump stream, pid: ${Process.myPid()}] Wait dump complete (dump process pid: ${pid})."
                    )
                    val result = wait(pid)
                    MatrixLog.i(
                        TAG,
                        "[dump stream, pid: ${Process.myPid()}] Dump complete, status code: $result."
                    )
                    result == 0
                }
                DumpHandler(stream, future)
            }
        }
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

/*
    Native Function Handlers.
 */

private fun initialize(): Boolean {
    System.loadLibrary("matrix-memorydump")
    val success = initializeNative()
    if (!success) {
        MatrixLog.printErrStackTrace(
            TAG,
            RuntimeException("Failed to initialize native resources."),
            ""
        )
    }
    return success
}

/**
 * Initialize native resources.
 */
private external fun initializeNative(): Boolean

/**
 * Dump HPROF file to specific file which file descriptor [fd] points to. The function returns 0 if
 * dump successfully, or -1 if error happened.
 *
 * The function is implemented with art::hprof::DumpHeap() in libart.so.
 */
private external fun dumpHprof(fd: Int): Int

/**
 * Fork dump process. To solve the deadlock, this function will suspend the runtime and resume it in
 * parent process.
 *
 * The function is implemented with native standard fork().
 * See [man fork](https://man7.org/linux/man-pages/man2/fork.2.html).
 */
private external fun fork(timeout: Long): Int

/**
 * Fork dump process with a pipe for transferring data. To solve the deadlock, this function will
 * suspend the runtime and resume it in parent process.
 *
 * The code this function may returns:
 * -1: Cannot fork child dump process.
 * -2: Failed to allocate native resource in parent process.
 * others: The return value is a pointer of integer pair in native. The integer pair is "forked dump
 *      process id - hprof-reading file descriptor".
 *
 * Reason for designing the return value:
 * The runtime cannot allocate any object after invoking fork() in forked process.
 *
 * The function is implemented with native standard fork() and pipe().
 * See [man fork](https://man7.org/linux/man-pages/man2/fork.2.html)
 * and [man pipe](https://man7.org/linux/man-pages/man2/pipe.2.html).
 */
private external fun forkPipe(timeout: Long): Long

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

/**
 * Get process id member value of native fork_pair instance by [pointer].
 *
 * Reason for designing structure in native:
 * The runtime cannot allocate any object after invoking fork() in forked process.
 */
private external fun pidFromForkPair(pointer: Long): Int

/**
 * Get file descriptor member value of native fork_pair instance by [pointer].
 *
 * Reason for designing structure in native:
 * The runtime cannot allocate any object after invoking fork() in forked process.
 */
private external fun fdFromForkPair(pointer: Long): Int

/**
 * Free the native memory space pointed to by [pointer].
 *
 * The function is implemented with native standard free().
 * See [man free](https://man7.org/linux/man-pages/man3/free.3.html).
 */
private external fun free(pointer: Long)