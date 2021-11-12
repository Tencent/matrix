package com.tencent.matrix.lifecycle.supervisor

import android.app.ActivityManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Process
import com.tencent.matrix.lifecycle.StatefulOwner
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil
import java.util.*
import java.util.concurrent.ConcurrentHashMap
import kotlin.collections.ArrayList

/**
 * [SupervisorReceiver] should be installed in the supervisor process only
 * by calling [ProcessSupervisor.inCharge]
 *
 * Created by Yves on 2021/10/12
 */
@Deprecated("")
internal object SupervisorReceiver : BroadcastReceiver() {

    private const val TAG = "Matrix.ProcessSupervisor.SupervisorReceiver"

    private enum class ProcessEvent {
        SUPERVISOR_PROCESS_CREATED, // if the supervisor process were not main process, the create event would be lost
        SUPERVISOR_PROCESS_FOREGROUNDED,
        SUPERVISOR_PROCESS_BACKGROUNDED,
        SUPERVISOR_PROCESS_CHECK_VALID,
        SUPERVISOR_PROCESS_KILLED,
        SUPERVISOR_PROCESS_KILL_RESCUED,
        SUPERVISOR_PROCESS_KILL_CANCELED,
        CHECK_SUPERVISOR_REGISTER,
        CHECK_ERROR_DAMAGE;
    }

    private class RemoteProcessLifecycleProxy(val processName: String, val pid: Int) :
        StatefulOwner() {

        init {
            ProcessSupervisor.addSourceOwner(this)
        }

        companion object {
            private val processFgObservers by lazy { ConcurrentHashMap<String, RemoteProcessLifecycleProxy>() }

            fun getProxy(processName: String?, pid: Int?): RemoteProcessLifecycleProxy? {
                return processName?.run {
                    processFgObservers.getOrPut(this, { RemoteProcessLifecycleProxy(this, pid!!) })
                }
            }

            fun removeProxy(processName: String) {
                processFgObservers.remove(processName)?.let { statefulOwner ->
                    ProcessSupervisor.removeSourceOwner(statefulOwner)
                }
            }

            fun checkValid() {
                val begin = System.currentTimeMillis()
                val runningProcesses = ams?.runningAppProcesses

                val got = System.currentTimeMillis()

                MatrixLog.d(TAG, "get running processes cost ${System.currentTimeMillis() - begin}")

                processFgObservers.values.forEach {
                    it.checkValid(runningProcesses)
                }

                MatrixLog.d(TAG, "get running processes cost ${System.currentTimeMillis() - got} for each check cost ${System.currentTimeMillis() - got}")
            }
        }

        private fun checkValid(runningProcesses : List<ActivityManager.RunningAppProcessInfo>?) = runningProcesses?.any {
            pid == it.pid && processName == it.processName
        } ?: false.also {
            removeProxy(processName)
            backgroundProcessLru?.remove(ProcessInfo(processName, pid))
            MatrixLog.e(TAG, "$it is invalid")
        }

        fun onRemoteProcessForeground() = turnOn()
        fun onRemoteProcessBackground() = turnOff()
        override fun toString(): String {
            return "OwnerProxy_${processName}_$pid"
        }
    }

    data class ProcessInfo  (val name: String, val pid: Int) {
        override fun toString(): String {
            return "$pid-$name"
        }

        override fun equals(other: Any?): Boolean {
            if (other == null) {
                return false
            }
            if (other !is ProcessInfo) {
                return false
            }

            return name == other.name
        }

        override fun hashCode(): Int {
            return name.hashCode()
        }
    }

    private val pendingEvents = ArrayList<ProcessEvent>()

    private var backgroundProcessLru: LinkedList<ProcessInfo>? = null
        get() {
            if (!isSupervisor) {
                throw UnsupportedOperationException("NOT support for other processes")
            }
            return field
        }

    private fun LinkedList<ProcessInfo>.moveOrAddFirst(info: ProcessInfo) {
        remove(info)
        addFirst(info)
    }

    internal var supervisorProcessName: String? = null

    internal val isSupervisor: Boolean
        get() = supervisorProcessName != null

    private var targetKilledCallback: ((result: Int, target: String?) -> Unit)? = null

    @Volatile
    private var isSupervisorInstalled: Boolean = false

    private var ams: ActivityManager? = null

    internal fun install(context: Context?) {
        backgroundProcessLru = LinkedList<ProcessInfo>()

        val filter = IntentFilter()
        ProcessEvent.values().forEach {
            filter.addAction(it.name)
        }
        context?.registerReceiver(
            this, filter,
            ProcessSupervisor.permission, null
        )
        ams = context?.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager

        checkUnique(context)

        flushPending(context)

//        MatrixLog.i(TAG, "RegisterReceiver installed")
    }

    internal fun sendOnProcessInit(context: Context?) {
        send(context, ProcessEvent.SUPERVISOR_PROCESS_CREATED)
    }

    internal fun sendOnProcessForeground(context: Context?) {
        send(context, ProcessEvent.SUPERVISOR_PROCESS_FOREGROUNDED)
    }

    internal fun sendOnProcessBackground(context: Context?) {
        send(context, ProcessEvent.SUPERVISOR_PROCESS_BACKGROUNDED)
    }

    internal fun sendOnProcessKilled(context: Context?) {
        send(context, ProcessEvent.SUPERVISOR_PROCESS_KILLED)
    }

    internal fun sendOnProcessKillRescued(context: Context?) {
        send(context, ProcessEvent.SUPERVISOR_PROCESS_KILL_RESCUED)
    }

    internal fun sendOnProcessKillCanceled(context: Context?) {
        send(context, ProcessEvent.SUPERVISOR_PROCESS_KILL_CANCELED)
    }

    internal fun triggerCheckValid(context: Context?) {
        send(context, ProcessEvent.SUPERVISOR_PROCESS_CHECK_VALID)
    }

    internal fun backgroundLruKill(
        context: Context?,
        killedCallback: (result: Int, process: String?) -> Unit
    ) {
        if (!isSupervisor) {
            throw IllegalStateException("backgroundLruKill should only be called in supervisor")
        }

        RemoteProcessLifecycleProxy.checkValid()

        targetKilledCallback = killedCallback
        backgroundProcessLru?.lastOrNull {
            it.name != MatrixUtil.getProcessName(context)
        }?.let {
            DispatchReceiver.dispatchKill(context, it.name, it.pid)
        } ?: let {
            killedCallback.invoke(LRU_KILL_NOT_FOUND, null)
        }
    }

    private fun checkUnique(context: Context?) {
        send(context, ProcessEvent.CHECK_SUPERVISOR_REGISTER)
    }

    private fun send(context: Context?, event: ProcessEvent) {
        if (isSupervisorInstalled || event == ProcessEvent.SUPERVISOR_PROCESS_CREATED) {
            sendImpl(context, event)
        } else {
            pendingSend(event)
        }
    }

    // call from all process
    private fun sendImpl(context: Context?, event: ProcessEvent) {
        val processName = MatrixUtil.getProcessName(context)
//        MatrixLog.d(SupervisorLifecycle.tag, "send [${event.name}] from [$processName]")
        val intent = Intent(event.name)
        intent.putExtra(KEY_PROCESS_NAME, processName)
        intent.putExtra(KEY_PROCESS_PID, Process.myPid())
        intent.putExtra(KEY_PROCESS_BUSY, false)

        if (isSupervisor) {
//            MatrixLog.d(TAG, "send direct")
            onReceive(context, intent)
            return
        }

        context?.sendBroadcast(
            intent, ProcessSupervisor.permission
        )
    }

    private fun pendingSend(event: ProcessEvent) {
//        MatrixLog.i(TAG, "pending...[${event.name}]")
        pendingEvents.add(event)
    }

    internal fun flushPending(context: Context?) {
//        MatrixLog.d(TAG, "flushPending...")
        isSupervisorInstalled = true
        pendingEvents.forEach {
            sendImpl(context, it)
        }
        pendingEvents.clear()
    }

    private fun asyncLog(t: String?, format: String?, vararg obj: Any?) {
        MatrixHandlerThread.getDefaultHandler().post {
            MatrixLog.i(t, format, *obj)
        }
    }

    override fun onReceive(context: Context?, intent: Intent?) {
        if (intent == null) {
            return
        }
        val processName = intent.getStringExtra(KEY_PROCESS_NAME)
        val pid = intent.getIntExtra(KEY_PROCESS_PID, 0)
//        MatrixLog.d(SupervisorLifecycleOwner.tag, "Action [${intent?.action}] [$processName]")

        val proxy = RemoteProcessLifecycleProxy.getProxy(processName, pid)

        when (intent.action) {
//            ProcessEvent.SUPERVISOR_PROCESS_CREATED.name,
//            ProcessEvent.SUPERVISOR_PROCESS_FOREGROUNDED.name,
//            ProcessEvent.SUPERVISOR_PROCESS_BACKGROUNDED.name,
//            ProcessEvent.SUPERVISOR_PROCESS_CHECK_VALID.name -> {
//                MatrixLog.d(TAG, "check valid onReceive ${intent.action}")
//                RemoteProcessLifecycleProxy.checkValid()
//            }
            ProcessEvent.SUPERVISOR_PROCESS_CREATED.name -> {
                processName?.let {
                    backgroundProcessLru?.moveOrAddFirst(ProcessInfo(it, pid))
                    // dispatch again for new process
                    if (processName != supervisorProcessName) {
                        if (ProcessSupervisor.active()) {
                            DispatchReceiver.dispatchAppForeground(context)
                        } else {
                            DispatchReceiver.dispatchAppBackground(context)
                        }
                    }

                    asyncLog(
                        TAG,
                        "CREATED: [$pid-$it] -> [${backgroundProcessLru?.size}]${backgroundProcessLru.toString()}"
                    )
                }
            }
            ProcessEvent.SUPERVISOR_PROCESS_BACKGROUNDED.name -> {
                processName?.let {
                    backgroundProcessLru?.moveOrAddFirst(ProcessInfo(it, pid))
                    proxy?.onRemoteProcessBackground()
                    asyncLog(
                        TAG,
                        "BACKGROUND: [$pid-$it] -> [${backgroundProcessLru?.size}]${backgroundProcessLru.toString()}"
                    )
                }
            }
            ProcessEvent.SUPERVISOR_PROCESS_FOREGROUNDED.name -> {
                processName?.let {
                    backgroundProcessLru?.remove(ProcessInfo(it, pid))
                    proxy?.onRemoteProcessForeground()
                    asyncLog(
                        TAG,
                        "FOREGROUND: [$pid-$it] <- [${backgroundProcessLru?.size}]${backgroundProcessLru.toString()}"
                    )
                }
            }
            ProcessEvent.SUPERVISOR_PROCESS_KILLED.name -> {
                processName?.let { name ->
                    try {
                        targetKilledCallback?.invoke(LRU_KILL_SUCCESS, name)
                    } catch (ignore: Throwable) {
                    }
                    backgroundProcessLru?.remove(ProcessInfo(name, pid))
                    RemoteProcessLifecycleProxy.removeProxy(name)
                    asyncLog(
                        TAG,
                        "KILL: [$pid-$name] X [${backgroundProcessLru?.size}]${backgroundProcessLru.toString()}"
                    )
                }
            }
            ProcessEvent.SUPERVISOR_PROCESS_KILL_RESCUED.name -> {
                try {
                    targetKilledCallback?.invoke(LRU_KILL_RESCUED, null)
                } catch (ignore: Throwable) {
                }
            }
            ProcessEvent.SUPERVISOR_PROCESS_KILL_CANCELED.name -> {
                try {
                    targetKilledCallback?.invoke(LRU_KILL_CANCELED, null)
                } catch (ignore: Throwable) {
                }
            }
            ProcessEvent.CHECK_SUPERVISOR_REGISTER.name -> {
                if (processName == MatrixUtil.getProcessName(context)) {
                    MatrixLog.e(TAG, "ignore check self [$processName]")
                    return
                }
                send(context, ProcessEvent.CHECK_ERROR_DAMAGE)
            }
            ProcessEvent.CHECK_ERROR_DAMAGE.name -> {
                context?.unregisterReceiver(this)
                "IllegalState!!! more than one supervisor!!!".let {
                    MatrixLog.e(TAG, it)
                    throw IllegalStateException(it)
                }
            }
        }
    }
}