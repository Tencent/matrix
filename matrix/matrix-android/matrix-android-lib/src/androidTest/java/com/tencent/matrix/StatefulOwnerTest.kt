package com.tencent.matrix

import android.os.SystemClock
import android.util.Log
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.tencent.matrix.lifecycle.*
import com.tencent.matrix.util.MatrixLog
import junit.framework.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Created by Yves on 2021/11/11
 */
@RunWith(AndroidJUnit4::class)
class StatefulOwnerTest {

    companion object {
        private const val TAG = "Matrix.test.StatefulOwnerTest"
    }

    @Test
    fun testRemoveSource() {

        class TestMsOwner: MultiSourceStatefulOwner(ReduceOperators.OR)

        class TestStatefulOwner: StatefulOwner() {
            fun handleOn() = turnOn()
            fun handleOff() = turnOff()
        }

        val msOwner = TestMsOwner()
        msOwner.observeForever(object : IStateObserver {
            override fun on() {
                MatrixLog.d(TAG, "test ON")
            }

            override fun off() {
                MatrixLog.d(TAG, "test OFF")
            }

        })
        assertEquals(msOwner.active(), false)

        val s1 = TestStatefulOwner().apply {
            handleOn()
            MatrixLog.d(TAG, "add s1")
            msOwner.addSourceOwner(this)
        }

        assertEquals(msOwner.active(), true)

        val s2 = TestStatefulOwner().apply {
            handleOff()
            MatrixLog.d(TAG, "add s2")
            msOwner.addSourceOwner(this)
        }

        assertEquals(msOwner.active(), true)

        MatrixLog.d(TAG, "remove s1")
        msOwner.removeSourceOwner(s1)

        assertEquals(msOwner.active(), false)

        MatrixLog.d(TAG, "turn off s2")
        s2.handleOff()

    }

    @Test
    fun scheduleTest() {
        val o1 = object : StatefulOwner(true) {
            fun handleOn() = turnOn()
            fun handleOff() = turnOff()
        }

        o1.observeForever(object : IStateObserver {
            override fun on() {
                Log.d(TAG, "on: normal observe at ${Thread.currentThread().name}")
            }

            override fun off() {
                Log.d(TAG, "off: normal observe at ${Thread.currentThread().name}")
            }
        })

        o1.observeForever(object : ISerialObserver {
            override fun on() {
                Log.d(TAG, "on: serial observe at ${Thread.currentThread().name} pool size = ${MatrixLifecycleThread.executor.poolSize}")
            }

            override fun off() {
                Log.d(TAG, "off: serial observe at ${Thread.currentThread().name} pool size = ${MatrixLifecycleThread.executor.poolSize}")
            }
        })

        o1.handleOn()
        o1.handleOff()
        SystemClock.sleep(100)
        o1.handleOn()
        o1.handleOff()
        SystemClock.sleep(100)
        o1.handleOn()
        o1.handleOff()

        val m1 = MultiSourceStatefulOwner(ReduceOperators.AND, o1)
        m1.observeForever(object : IStateObserver {
            override fun on() {
                Log.d(TAG, "Multi on: normal observe at ${Thread.currentThread().name}")
            }

            override fun off() {
                Log.d(TAG, "Multi off: normal observe at ${Thread.currentThread().name}")
            }
        })
        m1.observeForever(object : ISerialObserver {
            override fun on() {
                Log.d(TAG, "Multi on: serial observe at ${Thread.currentThread().name} pool size = ${MatrixLifecycleThread.executor.poolSize}")
            }

            override fun off() {
                Log.d(TAG, "Multi off: serial observe at ${Thread.currentThread().name} pool size = ${MatrixLifecycleThread.executor.poolSize}")
            }
        })

        o1.handleOn()
        o1.handleOff()
    }

}