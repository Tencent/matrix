package com.tencent.matrix.memory.canary.lifecycle.owners

import android.app.Application
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Process
import com.tencent.matrix.util.MatrixUtil
import com.tencent.matrix.memory.canary.lifecycle.StatefulOwner
import com.tencent.matrix.memory.canary.lifecycle.MultiSourceStatefulOwner
import com.tencent.matrix.memory.canary.lifecycle.ReduceOperators
import com.tencent.matrix.memory.canary.lifecycle.IStateObserver
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import java.util.*
import java.util.concurrent.TimeUnit
import kotlin.NoSuchElementException
import kotlin.collections.ArrayList
import kotlin.collections.HashMap

/**
 * Created by Yves on 2021/9/26
 */
// TODO: 2021/10/11 代码抽离
object ProcessSupervisor :
    MultiSourceStatefulOwner(ReduceOperators.OR) {

    internal val tag by lazy { "Matrix.memory.Supervisor_${suffix()}" }

    private fun suffix(): String {
        return if (MatrixUtil.isInMainProcess(application)) {
            "Main"
        } else {
            val split = MatrixUtil.getProcessName(application).split(":").toTypedArray()
            if (split.size > 1) {
                split[1]
            } else {
                "unknown"
            }
        }
    }

    private var application: Application? = null

    /**
     * should be call only once in process with the maximum life span
     */
    fun initSupervisor(process: String, app: Application) {
        application = app
        SupervisorReceiver.supervisorProcessName = MatrixUtil.getProcessName(application)
        if (process == SupervisorReceiver.supervisorProcessName) { // ur the chosen one
            SupervisorReceiver.install(application)

            observeForever(object : IStateObserver {

                override fun on() {
                    DispatchReceiver.dispatchAppForeground(application)
                }

                override fun off() {
                    DispatchReceiver.dispatchAppBackground(application)
                }
            })
        }
    }

    /**
     * call by all processes
     */
    fun inCharge(app: Application) {
        application = app

        DispatchReceiver.install(application)

        SupervisorReceiver.sendOnProcessInit(application)

        CombinedProcessForegroundStatefulOwner.observeForever(object : IStateObserver {

            override fun on() {
                MatrixLog.d(tag, "CombinedForegroundProcessLifecycle: ON")
                SupervisorReceiver.sendOnProcessForeground(application)
            }

            override fun off() {
                MatrixLog.d(tag, "CombinedForegroundProcessLifecycle: OFF")
                SupervisorReceiver.sendOnProcessBackground(application)
            }
        })

        // for Test
//        MultiProcessLifecycleOwner.get().lifecycle.addObserver(object : LifecycleObserver {
//            @OnLifecycleEvent(Lifecycle.Event.ON_START)
//            fun onCurrentProcessForeground() {
////                MatrixLog.d(tag, "onCurrentProcessForeground")
//                SupervisorReceiver.sendOnProcessForeground(application)
//            }
//
//            @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
//            fun onCurrentProcessBackground() {
////                MatrixLog.d(tag, "onCurrentProcessBackground")
//                SupervisorReceiver.sendOnProcessBackground(application)
//            }
//        })
    }

    fun backgroundLruKill(): Boolean {
        try {
            SupervisorReceiver.backgroundProcessLru?.last {
                it != MatrixUtil.getProcessName(application)
            }?.let {
                DispatchReceiver.dispatchKill(application, it)
                return true
            }
        } catch (ignore: NoSuchElementException) {
            return false
        }
        return false
    }

    val isAppForeground: Boolean
        get() = active()

    val isSupervisor: Boolean
        get() = SupervisorReceiver.isSupervisor

    internal fun syncAppForeground() {
        turnOn()
    }

    internal fun syncAppBackground() {
        turnOff()
    }
}

private const val SUPERVISOR_PERMISSION = "com.tencent.matrix.permission.MEMORY_CANARY"
private const val KEY_PROCESS_NAME = "KEY_PROCESS_NAME"

/**
 * running in supervisor process
 */
private object SupervisorReceiver : BroadcastReceiver() {

    private const val TAG = "MicroMsg.memory.SupervisorReceiver"

    private enum class ProcessEvent {
        SUPERVISOR_PROCESS_CREATED, // if the supervisor process were not main process, the create event would be lost
        SUPERVISOR_PROCESS_FOREGROUNDED,
        SUPERVISOR_PROCESS_BACKGROUNDED,
        SUPERVISOR_PROCESS_DESTROYED,
        CHECK_SUPERVISOR_REGISTER,
        CHECK_ERROR_DAMAGE;
    }

    private class RemoteProcessLifecycleProxy(val processName: String) : StatefulOwner() {
        init {
            ProcessSupervisor.addSourceOwner(this)
        }

        fun onRemoteProcessForeground() = turnOn()
        fun onRemoteProcessBackground() = turnOff()
        override fun toString(): String {
            return "OwnerProxy_${processName}"
        }
    }

    private val pendingEvents = ArrayList<ProcessEvent>()

    private val processFgObservers by lazy { HashMap<String, RemoteProcessLifecycleProxy>() }

    var backgroundProcessLru: LinkedList<String>? = null
        private set

    private fun LinkedList<String>.moveOrAddFirst(str: String) {
        remove(str)
        addFirst(str)
    }

    var supervisorProcessName: String? = null

    val isSupervisor: Boolean
        get() = supervisorProcessName != null

    @Volatile
    private var isSupervisorInstalled: Boolean = false

    fun install(context: Context?) {
        backgroundProcessLru = LinkedList<String>()

        val filter = IntentFilter()
        ProcessEvent.values().forEach {
            filter.addAction(it.name)
        }
        context?.registerReceiver(
            this, filter,
            SUPERVISOR_PERMISSION, null
        )

        checkUnique(context)

        DispatchReceiver.dispatchSuperVisorInstalled(context)

//        MatrixLog.i(TAG, "RegisterReceiver installed")
    }

    fun sendOnProcessInit(context: Context?) {
        send(context, ProcessEvent.SUPERVISOR_PROCESS_CREATED)
    }

    fun sendOnProcessForeground(context: Context?) {
        send(context, ProcessEvent.SUPERVISOR_PROCESS_FOREGROUNDED)
    }

    fun sendOnProcessBackground(context: Context?) {
        send(context, ProcessEvent.SUPERVISOR_PROCESS_BACKGROUNDED)
    }

    fun sendOnProcessDestroying(context: Context?) {
        send(context, ProcessEvent.SUPERVISOR_PROCESS_DESTROYED)
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

        if (isSupervisor) {
//            MatrixLog.d(TAG, "send direct")
            onReceive(context, intent)
        }

        context?.sendBroadcast(
            intent, SUPERVISOR_PERMISSION
        )
    }

    private fun pendingSend(event: ProcessEvent) {
//        MatrixLog.i(TAG, "pending...[${event.name}]")
        pendingEvents.add(event)
    }

    fun flushPending(context: Context?) {
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
        val processName = intent?.getStringExtra(KEY_PROCESS_NAME)
//        MatrixLog.d(SupervisorLifecycleOwner.tag, "Action [${intent?.action}] [$processName]")

        val proxy =
            processName?.run {
                processFgObservers.getOrPut(this, { RemoteProcessLifecycleProxy(processName) })
            }

        when (intent?.action) {
            ProcessEvent.SUPERVISOR_PROCESS_CREATED.name -> {
                processName?.let {
                    backgroundProcessLru?.moveOrAddFirst(it)
                    DispatchReceiver.dispatchSuperVisorInstalled(context)
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
                        "[$it] -> [${backgroundProcessLru?.size}]${backgroundProcessLru.toString()}"
                    )
                }
            }
            ProcessEvent.SUPERVISOR_PROCESS_BACKGROUNDED.name -> {
                processName?.let {
                    backgroundProcessLru?.moveOrAddFirst(it)
                    proxy?.onRemoteProcessBackground()
                    asyncLog(
                        TAG,
                        "[$it] -> [${backgroundProcessLru?.size}]${backgroundProcessLru.toString()}"
                    )
                }
            }
            ProcessEvent.SUPERVISOR_PROCESS_FOREGROUNDED.name -> {
                processName?.let {
                    backgroundProcessLru?.remove(it)
                    proxy?.onRemoteProcessForeground()
                    asyncLog(
                        TAG,
                        "[$it] <- [${backgroundProcessLru?.size}]${backgroundProcessLru.toString()}"
                    )
                }
            }
            ProcessEvent.SUPERVISOR_PROCESS_DESTROYED.name -> {
                processName?.let { name ->
                    backgroundProcessLru?.remove(name)
                    processFgObservers.remove(name)?.let { statefulOwner ->
                        ProcessSupervisor.removeSourceOwner(statefulOwner)
                    }
                    asyncLog(
                        TAG,
                        "[$name] X [${backgroundProcessLru?.size}]${backgroundProcessLru.toString()}"
                    )
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

private object DispatchReceiver : BroadcastReceiver() {

    private enum class SupervisorEvent {
        SUPERVISOR_INSTALLED,
        SUPERVISOR_DISPATCH_APP_FOREGROUND,
        SUPERVISOR_DISPATCH_APP_BACKGROUND,
        SUPERVISOR_DISPATCH_KILL;
    }

    fun install(context: Context?) {
        if (SupervisorReceiver.isSupervisor) {
            return
        }
        val filter = IntentFilter()
        SupervisorEvent.values().forEach {
            filter.addAction(it.name)
        }
        context?.registerReceiver(
            this, filter,
            SUPERVISOR_PERMISSION, null
        )
//        MatrixLog.i(SupervisorLifecycleOwner.tag, "DispatchReceiver installed")
    }

    fun dispatchSuperVisorInstalled(context: Context?) {
        dispatch(context, SupervisorEvent.SUPERVISOR_INSTALLED)
    }

    fun dispatchAppForeground(context: Context?) {
        dispatch(context, SupervisorEvent.SUPERVISOR_DISPATCH_APP_FOREGROUND)
    }

    fun dispatchAppBackground(context: Context?) {
        dispatch(context, SupervisorEvent.SUPERVISOR_DISPATCH_APP_BACKGROUND)
    }

    fun dispatchKill(context: Context?, targetProcessName: String) {
        dispatch(
            context,
            SupervisorEvent.SUPERVISOR_DISPATCH_KILL,
            Pair(KEY_PROCESS_NAME, targetProcessName)
        )
    }

    // call from supervisor process
    private fun dispatch(
        context: Context?,
        event: SupervisorEvent,
        vararg params: Pair<String, String>
    ) {
//        MatrixLog.d(SupervisorLifecycle.tag, "dispatch [${event.name}]")
        val intent = Intent(event.name)
        params.forEach {
            intent.putExtra(it.first, it.second)
        }
        context?.sendBroadcast(intent, SUPERVISOR_PERMISSION)
    }

    override fun onReceive(context: Context?, intent: Intent?) {
//        MatrixLog.d(SupervisorLifecycle.tag, "DispatchReceiver on onReceive [${intent?.action}]")
        when (intent?.action) {
            SupervisorEvent.SUPERVISOR_INSTALLED.name -> {
                SupervisorReceiver.flushPending(context)
            }
            SupervisorEvent.SUPERVISOR_DISPATCH_APP_FOREGROUND.name -> {
                ProcessSupervisor.syncAppForeground()
            }
            SupervisorEvent.SUPERVISOR_DISPATCH_APP_BACKGROUND.name -> {
                ProcessSupervisor.syncAppBackground()
            }
            SupervisorEvent.SUPERVISOR_DISPATCH_KILL.name -> {
                val target = intent.getStringExtra(KEY_PROCESS_NAME)
                MatrixLog.d(ProcessSupervisor.tag, "receive kill target: $target")
                if (target == MatrixUtil.getProcessName(context)) {
                    SupervisorReceiver.sendOnProcessDestroying(context)
                    MatrixHandlerThread.getDefaultHandler().postDelayed({
                        if (!CombinedProcessForegroundStatefulOwner.active()) {
                            MatrixLog.e(ProcessSupervisor.tag, "actual kill !!!")
                            Process.killProcess(Process.myPid())
                        } else {
                            MatrixLog.i(ProcessSupervisor.tag, "recheck: process is on foreground")
                        }
                    }, TimeUnit.SECONDS.toMillis(10))
                }
            }
        }
    }
}
