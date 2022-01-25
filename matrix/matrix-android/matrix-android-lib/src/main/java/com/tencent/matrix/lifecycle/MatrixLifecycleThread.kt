package com.tencent.matrix.lifecycle

import android.os.Handler
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import java.util.*
import java.util.concurrent.*

/**
 * Created by Yves on 2022/1/11
 */
internal object MatrixLifecycleThread {

    private const val TAG = "Matrix.Lifecycle.Thread"

    private const val CORE_POOL_SIZE = 0
    private const val MAX_POOL_SIZE = 5
    private const val KEEP_ALIVE_SECONDS = 30L

    val handler by lazy(LazyThreadSafetyMode.SYNCHRONIZED) {
        Handler(MatrixHandlerThread.getNewHandlerThread("matrix_li", Thread.NORM_PRIORITY).looper)
    }

    private val workers = WeakHashMap<Thread, Any>()

    val executor by lazy(LazyThreadSafetyMode.SYNCHRONIZED) {
        val idleSynchronousQueue = IdleSynchronousQueue()

        object : ThreadPoolExecutor(
            CORE_POOL_SIZE,
            MAX_POOL_SIZE,
            KEEP_ALIVE_SECONDS,
            TimeUnit.SECONDS,
            idleSynchronousQueue,
            { r ->
                Thread(Thread.currentThread().threadGroup, r, "matrix_x_${workers.size}", 0).also {
                    workers[it] = Any()
                }
            },
            { r, _ -> // full now
                idleSynchronousQueue.idle(r)
            }
        ) {
            override fun execute(command: Runnable?) {
                super.execute(command?.wrap())
            }
        }
    }

    private data class TaskInfo(val task: String = "", val time: Long = 0) {
        companion object {
            fun of(runnable: Runnable?) = if (runnable == null) {
                TaskInfo("", System.currentTimeMillis())
            } else {
                TaskInfo(runnable.toString(), System.currentTimeMillis())
            }
        }
    }

    private val lastIdleTaskOfThread = ConcurrentHashMap<Thread, TaskInfo>()

    private fun ConcurrentHashMap<Thread, TaskInfo>.checkTimeout() {
        forEach {
            if (it.value.task.isEmpty()) {
                return@forEach
            }
            val cost = System.currentTimeMillis() - it.value.time
//            MatrixLog.d(TAG, "-> ${it.value.task} -> $cost")
            if (cost > TimeUnit.SECONDS.toMillis(30)) {
                buildString {
                    append("Dispatcher Thread Not Responding : ${it.key.name}\n")
                    it.key.stackTrace.forEach { s ->
                        append("\tat $s\n")
                    }
                }.let { stacktrace ->
                    // TODO: 2022/1/25 profiling
                    MatrixLog.e(TAG, stacktrace)
                }
            }
        }
    }

    private fun Runnable.wrap() = object : Runnable {

        override fun run() {
            val begin = System.currentTimeMillis()

            TaskInfo.of(this@wrap).let {
                lastIdleTaskOfThread[Thread.currentThread()] = it
            }

            this@wrap.run()

            lastIdleTaskOfThread[Thread.currentThread()] = TaskInfo.of(null)

            val cost = System.currentTimeMillis() - begin
            if (cost > 500) {
                // TODO: 2022/1/25 profiling
                MatrixLog.e(TAG, "heavy task(cost ${cost}ms):$this")
            }
        }

        override fun toString(): String {
            return this@wrap.toString()
        }
    }

    private class IdleSynchronousQueue : SynchronousQueue<Runnable>() {
        private val idleQueue = LinkedBlockingQueue<Runnable>()

        override fun poll(timeout: Long, unit: TimeUnit?): Runnable? {
            return idleQueue.poll() ?: super.poll(timeout, unit)
        }

        override fun poll(): Runnable? {
            return idleQueue.poll() ?: super.poll()
        }

        override fun take(): Runnable {
            return idleQueue.poll() ?: super.take()
        }

        fun idle(r: Runnable) {
            lastIdleTaskOfThread.checkTimeout()
            idleQueue.offer(r)
        }
    }

}