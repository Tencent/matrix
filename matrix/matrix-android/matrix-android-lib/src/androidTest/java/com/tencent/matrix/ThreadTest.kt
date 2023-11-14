package com.tencent.matrix

import android.os.SystemClock
import android.util.Log
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.tencent.matrix.lifecycle.MatrixLifecycleThread
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.*

/**
 * Created by Yves on 2022/1/17
 */
@RunWith(AndroidJUnit4::class)
class ThreadTest {
    companion object {
        private const val TAG = "ThreadTest"
    }

    @Test
    fun executorTest() {
        Log.d(TAG, "executorTest: - 1")
        MatrixLifecycleThread.executor.execute {
//            SystemClock.sleep(1000)
            Log.d(TAG, "executorTest: 1 ${Thread.currentThread().name}")
            while (true) {}
        }

        SystemClock.sleep(1000)

        Log.d(TAG, "executorTest: - 2")
        MatrixLifecycleThread.executor.execute {
//            SystemClock.sleep(1000)
            Log.d(TAG, "executorTest: 2 ${Thread.currentThread().name}")
            while (true) {}
        }

        SystemClock.sleep(1000)

        Log.d(TAG, "executorTest: - 3")
        MatrixLifecycleThread.executor.execute {
//            SystemClock.sleep(1000)
            Log.d(TAG, "executorTest: 3 ${Thread.currentThread().name}")
            while (true) {}
        }

        SystemClock.sleep(1000)

        Log.d(TAG, "executorTest: - 4")
        MatrixLifecycleThread.executor.execute {
//            SystemClock.sleep(1000)
            Log.d(TAG, "executorTest: 4 ${Thread.currentThread().name}")
            while (true) {}
        }

        SystemClock.sleep(1000)

        Log.d(TAG, "executorTest: - 5 ${Thread.currentThread().name}")
        MatrixLifecycleThread.executor.execute {
//            SystemClock.sleep(1000)
            Log.d(TAG, "executorTest: 5 ${Thread.currentThread().name}")
            while (true) {}
        }

        SystemClock.sleep(1000)

        Log.d(TAG, "executorTest: - 6")
        MatrixLifecycleThread.executor.execute {
//            SystemClock.sleep(1000)
            Log.d(TAG, "executorTest: 6 ${Thread.currentThread().name}")
            while (true) {}
        }


        SystemClock.sleep(1000)

        Log.d(TAG, "executorTest: - 7")
        MatrixLifecycleThread.executor.execute {
//            SystemClock.sleep(1000)
            Log.d(TAG, "executorTest: 7 ${Thread.currentThread().name}")
            while (true) {}
        }

        SystemClock.sleep(1000)

        Log.d(TAG, "executorTest: - 8")
        MatrixLifecycleThread.executor.execute {
//            SystemClock.sleep(1000)
            Log.d(TAG, "executorTest: 8 ${Thread.currentThread().name}")
            while (true) {}
        }



        SystemClock.sleep(15000)
    }

    @Test
    fun forkJoinPoolTest() {
        val pool = ForkJoinPool(
            Runtime.getRuntime().availableProcessors(),
            { pool ->
                val thread = object : ForkJoinWorkerThread(pool) {}
                thread.name = "matrix_di_${(pool?.poolSize ?: 0) + 1}"
                Log.d(TAG, "forkJoinPoolTest: create ${thread.name}")
                thread
            },
            null,
            true
        )

        for (i in 0..10) {
            pool.execute {
                SystemClock.sleep(10)
                Log.d(TAG, "forkJoinPoolTest: task done")
            }
        }

        SystemClock.sleep(2)

        Log.d(TAG, "forkJoinPoolTest: ${pool.poolSize}")

        SystemClock.sleep(1000)

        Log.d(TAG, "forkJoinPoolTest: ${pool.poolSize}")
    }

    @Test
    fun executorTest2() {
        val c = MatrixLifecycleThread.executor

        c.execute {
            Log.d(TAG, "cachedThreadPoolTest: costly task")
            SystemClock.sleep(1000 * 100L)
            Log.d(TAG, "cachedThreadPoolTest: costly task done")
        }

        c.execute (object : Runnable { override fun run() { SystemClock.sleep(10); Log.d(TAG, "cachedThreadPoolTest: task 2, size = ${c.poolSize}, $this") } })
        c.execute (object : Runnable { override fun run() { SystemClock.sleep(10); Log.d(TAG, "cachedThreadPoolTest: task 3, size = ${c.poolSize}, $this") } })
        c.execute (object : Runnable { override fun run() { SystemClock.sleep(10); Log.d(TAG, "cachedThreadPoolTest: task 4, size = ${c.poolSize}, $this") } })
        c.execute (object : Runnable { override fun run() { SystemClock.sleep(10); Log.d(TAG, "cachedThreadPoolTest: task 5, size = ${c.poolSize}, $this") } })
        c.execute (object : Runnable { override fun run() { SystemClock.sleep(10); Log.d(TAG, "cachedThreadPoolTest: task 6, size = ${c.poolSize}, $this") } })
        c.execute (object : Runnable { override fun run() { SystemClock.sleep(10); Log.d(TAG, "cachedThreadPoolTest: task 7, size = ${c.poolSize}, $this") } })
        c.execute (object : Runnable { override fun run() { SystemClock.sleep(10); Log.d(TAG, "cachedThreadPoolTest: task 8, size = ${c.poolSize}, $this") } })
        c.execute (object : Runnable { override fun run() { SystemClock.sleep(10); Log.d(TAG, "cachedThreadPoolTest: task 9, size = ${c.poolSize}, $this") } })
        c.execute (object : Runnable { override fun run() { SystemClock.sleep(10); Log.d(TAG, "cachedThreadPoolTest: task 10, size = ${c.poolSize}, $this") } })
        c.execute (object : Runnable { override fun run() { SystemClock.sleep(510); Log.d(TAG, "cachedThreadPoolTest: task 11, size = ${c.poolSize}, $this") } })
        c.execute (object : Runnable { override fun run() { SystemClock.sleep(20000); Log.d(TAG, "cachedThreadPoolTest: task 12, size = ${c.poolSize}, $this") } })

        Log.d(TAG, "cachedThreadPoolTest: size 1 = ${c.poolSize}")

//        SystemClock.sleep(35 * 1000L)

        Log.d(TAG, "cachedThreadPoolTest: size 2 = ${c.poolSize}")

        SystemClock.sleep(15 * 1000L)

        Log.d(TAG, "cachedThreadPoolTest: size 3 = ${c.poolSize}")

        c.execute (object : Runnable { override fun run() { SystemClock.sleep(10); Log.d(TAG, "cachedThreadPoolTest: task 13, size = ${c.poolSize}, $this") } })
        c.execute (object : Runnable { override fun run() { SystemClock.sleep(10); Log.d(TAG, "cachedThreadPoolTest: task 14, size = ${c.poolSize}, $this") } })
        c.execute (object : Runnable { override fun run() { SystemClock.sleep(10); Log.d(TAG, "cachedThreadPoolTest: task 15, size = ${c.poolSize}, $this") } })
        c.execute (object : Runnable { override fun run() { SystemClock.sleep(10); Log.d(TAG, "cachedThreadPoolTest: task 16, size = ${c.poolSize}, $this") } })
        c.execute (object : Runnable { override fun run() { SystemClock.sleep(10); Log.d(TAG, "cachedThreadPoolTest: task 17, size = ${c.poolSize}, $this") } })
        c.execute (object : Runnable { override fun run() { SystemClock.sleep(10); Log.d(TAG, "cachedThreadPoolTest: task 18, size = ${c.poolSize}, $this") } })

        SystemClock.sleep(60 * 1000L)
    }

    @Test
    fun executorTest3() {
        val c = MatrixLifecycleThread.executor
        for (i in 0..10) {

            SystemClock.sleep(1000L)

            c.execute {
                SystemClock.sleep(1 * 1000L)
            }
        }

        SystemClock.sleep(30 * 1000L)
        Log.d(TAG, "=======")

        for (i in 0..10) {
            c.execute {
                SystemClock.sleep(10);
            }
        }

        SystemClock.sleep(1000 * 1000L)
    }
}