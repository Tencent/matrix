package com.tencent.matrix

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.MultiSourceStatefulOwner
import com.tencent.matrix.lifecycle.ReduceOperators
import com.tencent.matrix.lifecycle.StatefulOwner
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

        class TestStatefulOwner: StatefulOwner()

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
//            turnOn()
            MatrixLog.d(TAG, "add s1")
            msOwner.addSourceOwner(this)
        }

        assertEquals(msOwner.active(), true)

        val s2 = TestStatefulOwner().apply {
//            turnOn()
            MatrixLog.d(TAG, "add s2")
            msOwner.addSourceOwner(this)
        }

        assertEquals(msOwner.active(), true)

        MatrixLog.d(TAG, "remove s1")
        msOwner.removeSourceOwner(s1)

        assertEquals(msOwner.active(), false)

        MatrixLog.d(TAG, "turn off s2")
//        s2.turnOff()

    }

}