package com.tencent.matrix.memory.canary

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import com.tencent.matrix.Matrix
import com.tencent.matrix.lifecycle.SafeLifecycleRegistry
import com.tencent.matrix.memory.canary.lifecycle.IStateObserver
import com.tencent.matrix.memory.canary.lifecycle.owners.CombinedProcessForegroundStatefulOwner
import com.tencent.matrix.memory.canary.lifecycle.owners.DeepBackgroundStatefulOwner
import com.tencent.matrix.memory.canary.lifecycle.owners.ProcessSupervisor
import com.tencent.matrix.memory.canary.lifecycle.owners.StandbyStatefulOwner
import com.tencent.matrix.memory.canary.monitor.SumPssReportMonitor
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil

/**
 * Created by Yves on 2021/9/18
 */
object DefaultLifecycleBoot : MemoryCanaryInitializer() {

    private val TAG =
        "MicroMsg.lifecycle.MMAppLifecycleMonitor>>>>>> ${MatrixUtil.getProcessName(application)} >>>>>>>>>"

    override fun onInit() {
        super.onInit()

        CombinedProcessForegroundStatefulOwner.apply {
//                addSourceOwner(ForegroundServiceMonitor.instance.lifecycleOwner.toStateOwner())
            // TODO: 2021/10/11
        }
        DeepBackgroundStatefulOwner
        StandbyStatefulOwner

        val app = Matrix.with().application

        if (MatrixUtil.isInMainProcess(app)) {
            ProcessSupervisor.initSupervisor(
                MatrixUtil.getProcessName(app),
                app
            )
            SumPssReportMonitor.init()
        }
        ProcessSupervisor.inCharge(app)

        test()
        testLifecycle()
    }

    private fun test() {

        CombinedProcessForegroundStatefulOwner.observeForever(object : IStateObserver {

            override fun on() {
                MatrixLog.d(TAG, "[test] process foregrounded")
            }

            override fun off() {
                MatrixLog.d(TAG, "[test] process backgrounded")
            }
        })

        DeepBackgroundStatefulOwner.observeForever(object : IStateObserver {

            override fun on() {
                MatrixLog.d(TAG, "[test] deep backgrounded")
            }

            override fun off() {
                MatrixLog.d(TAG, "[test] exit deep backgrounded")
            }
        })

        StandbyStatefulOwner.observeForever(object : IStateObserver {

            override fun on() {
                MatrixLog.d(TAG, "[test] standby ")
            }

            override fun off() {
                MatrixLog.d(TAG, "[test] exit standby ")
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

    private fun testLifecycle() {
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