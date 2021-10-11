package com.tencent.matrix.memory.canary

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import com.tencent.matrix.lifecycle.SafeLifecycleRegistry
import com.tencent.matrix.memory.canary.lifecycle.IStateObserver
import com.tencent.matrix.memory.canary.lifecycle.owners.CombinedProcessForegroundStatefulOwner
import com.tencent.matrix.memory.canary.lifecycle.owners.ProcessSupervisor
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil

/**
 * Created by Yves on 2021/9/18
 */
object MemoryCanaryTest {

    private val TAG =
        "Matrix.MemoryCanaryTest >>>>>> ${MatrixUtil.getProcessName(DefaultMemoryCanaryInitializer.application)} >>>>>>>>>"

    internal fun test() {

        CombinedProcessForegroundStatefulOwner.observeForever(object : IStateObserver {

            override fun on() {
                MatrixLog.d(TAG, "[test] process foregrounded")
            }

            override fun off() {
                MatrixLog.d(TAG, "[test] process backgrounded")
            }
        })

        ProcessSupervisor.observeForever(object : IStateObserver {

            override fun on() {
                MatrixLog.d(TAG, "[test] onAppForeground ")
            }

            override fun off() {
                MatrixLog.d(TAG, "[test] onAppBackground ")
            }
        })
    }

    private object TestLifecycleOwner : LifecycleOwner {

        val registry = SafeLifecycleRegistry(this)

        override fun getLifecycle(): Lifecycle {
            return registry
        }

    }

    private object TestLifecycleOwner2 : LifecycleOwner {

        val registry = SafeLifecycleRegistry(this)

        override fun getLifecycle(): Lifecycle {
            return registry
        }

    }

    internal fun testLifecycle() {
        CombinedProcessForegroundStatefulOwner.observeWithLifecycle(TestLifecycleOwner, object :
            IStateObserver {
            override fun on() {
                MatrixLog.d(TAG, "TestLifecycleOwner -> observe ON")
            }

            override fun off() {
                MatrixLog.d(TAG, "TestLifecycleOwner -> observe OFF")
            }
        })

        try {
            val obj1 = object : IStateObserver {
                override fun on() {
                    MatrixLog.d(TAG, "obj1 on")
                }

                override fun off() {
                    MatrixLog.d(TAG, "obj1 off")
                }
            }

            val obj2 = object : IStateObserver {
                override fun on() {
                    MatrixLog.d(TAG, "obj2 on")
                }

                override fun off() {
                    MatrixLog.d(TAG, "obj2 off")
                }
            }

            CombinedProcessForegroundStatefulOwner.observeWithLifecycle(TestLifecycleOwner, obj1)
            CombinedProcessForegroundStatefulOwner.observeWithLifecycle(TestLifecycleOwner, obj1)
            CombinedProcessForegroundStatefulOwner.observeWithLifecycle(TestLifecycleOwner2, obj1)
            CombinedProcessForegroundStatefulOwner.observeWithLifecycle(TestLifecycleOwner2, obj2)
        } catch (e: Throwable) {
            MatrixLog.printErrStackTrace(TAG, e, "")
        }
    }

}