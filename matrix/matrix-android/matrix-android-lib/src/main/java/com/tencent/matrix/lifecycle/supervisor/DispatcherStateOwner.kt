package com.tencent.matrix.lifecycle.supervisor

import android.app.Application
import android.content.Context
import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.IStateful
import com.tencent.matrix.lifecycle.MultiSourceStatefulOwner
import com.tencent.matrix.lifecycle.StatefulOwner
import com.tencent.matrix.lifecycle.owners.ExplicitBackgroundOwner
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.safeApply
import java.lang.IllegalStateException

/**
 * cross process StatefulOwner
 *
 * DispatcherStateOwner sends attached owner state changes to supervisor
 * Supervisor sync the app states back through [dispatchOn] and [dispatchOff]
 *
 * Created by Yves on 2021/12/2
 */
@Suppress("LeakingThis")
internal open class DispatcherStateOwner(
    reduceOperator: (stateful: Collection<IStateful>) -> Boolean,
    val attachedSource: StatefulOwner,
    val name: String
) : MultiSourceStatefulOwner(reduceOperator) {

    companion object {
        private val dispatchOwners = HashMap<String, DispatcherStateOwner>()
        fun dispatchOn(name: String) {
            dispatchOwners[name]?.dispatchOn()
        }

        fun dispatchOff(name: String) {
            dispatchOwners[name]?.dispatchOff()
        }

        /**
         * call from supervisor
         */
        fun syncStates(context: Context?) {
            if (!ProcessSupervisor.isSupervisor) {
                throw IllegalStateException("call forbidden")
            }
            MatrixLog.i(ProcessSupervisor.tag, "syncStates")
            dispatchOwners.forEach {
                if (it.value.active()) {
                    DispatchReceiver.dispatchAppStateOn(context, it.key)
                } else {
                    DispatchReceiver.dispatchAppStateOn(context, it.key)
                }
            }
        }

        fun attach(supervisorProxy: ISupervisorProxy?, application: Application) {
            dispatchOwners.forEach {
                it.value.attachedSource.observeForever(object : IStateObserver {

                    override fun on() {
                        MatrixLog.d(ProcessSupervisor.tag, "${it.key} turned ON")
                        safeApply("${ProcessSupervisor.tag}.${it.key}") {
                            supervisorProxy?.stateTurnOn(
                                ProcessToken.current(application, it.key)
                            )
                        }
                    }

                    override fun off() {
                        MatrixLog.d(ProcessSupervisor.tag, "${it.key} turned OFF")
                        safeApply("${ProcessSupervisor.tag}.${it.key}") {
                            supervisorProxy?.stateTurnOff(
                                ProcessToken.current(application, it.key)
                            )
                        }
                    }
                })

                if (it.value.attachedSource is ExplicitBackgroundOwner) {
                    it.value.attachedSource.observeForever(object : IStateObserver {
                        override fun on() {
                            safeApply(ProcessSupervisor.tag) {
                                ProcessSupervisor.supervisorProxy?.onProcessBackground(
                                    ProcessToken.current(application)
                                )
                            }
                        }

                        override fun off() {
                            safeApply(ProcessSupervisor.tag) {
                                ProcessSupervisor.supervisorProxy?.onProcessForeground(
                                    ProcessToken.current(application)
                                )
                            }
                        }
                    })
                }
            }
        }

        fun observe(observer: (stateName: String, state: Boolean) -> Unit) {
            dispatchOwners.forEach {
                it.value.observeForever(object : IStateObserver {
                    override fun on() {
                        observer.invoke(it.key, true)
                    }

                    override fun off() {
                        observer.invoke(it.key, false)
                    }
                })
            }
        }

        fun addSourceOwner(name: String, source: StatefulOwner) {
            dispatchOwners[name]?.addSourceOwner(source)
        }

        fun removeSourceOwner(name: String, source: StatefulOwner) {
            dispatchOwners[name]?.removeSourceOwner(source)
        }
    }

    init {
        dispatchOwners[name] = this
    }

    private val h = MatrixHandlerThread.getDefaultHandler()
    private fun dispatchOn() = h.post { turnOn() }
    private fun dispatchOff() = h.post { turnOff() }
}