package com.tencent.matrix.lifecycle.owners

import android.annotation.SuppressLint
import android.app.ActivityManager
import android.app.Service
import android.content.ComponentName
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Message
import android.os.Process
import android.util.ArrayMap
import com.tencent.matrix.lifecycle.MatrixLifecycleThread
import com.tencent.matrix.lifecycle.StatefulOwner
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.safeApply
import com.tencent.matrix.util.safeLet
import com.tencent.matrix.util.safeLetOrNull
import java.lang.reflect.InvocationHandler
import java.lang.reflect.Method
import java.lang.reflect.Proxy

/**
 * Created by Yves on 2021/11/30
 */
object ForegroundServiceLifecycleOwner : StatefulOwner() {

    private const val TAG = "Matrix.lifecycle.FgService"

    private const val CREATE_SERVICE = 114
    private const val STOP_SERVICE = 116

    @SuppressLint("DiscouragedPrivateApi")
    private val fieldServicemActivityManager = safeLetOrNull(TAG) {
        Class.forName("android.app.Service").getDeclaredField("mActivityManager")
            .apply { isAccessible = true }
    }

    private var activityManager: ActivityManager? = null
    private var ActivityThreadmServices: ArrayMap<*, *>? = null
    private var ActivityThreadmH: Handler? = null

    private var fgServiceHandler: FgServiceHandler? = null

    fun init(context: Context, enable: Boolean) {
        activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        if (!enable) {
            MatrixLog.i(TAG, "disabled")
            return
        }
        inject()
    }

    @SuppressLint("DiscouragedPrivateApi", "PrivateApi")
    private fun inject() {

        safeApply(TAG) {

            val clazzActivityThread = Class.forName("android.app.ActivityThread")

            val fieldActivityThreadmH =
                clazzActivityThread.getDeclaredField("mH")
                    .apply { isAccessible = true }

            val activityThread = clazzActivityThread.getMethod("currentActivityThread").let {
                it.isAccessible = true
                it.invoke(null)
            }

            ActivityThreadmServices = clazzActivityThread.getDeclaredField("mServices").let {
                it.isAccessible = true
                it.get(activityThread) as ArrayMap<*, *>?
            }

            ActivityThreadmH = fieldActivityThreadmH.get(activityThread) as Handler

            Handler::class.java.getDeclaredField("mCallback").apply {
                    isAccessible = true
                    val origin = get(ActivityThreadmH) as Handler.Callback?
                    set(ActivityThreadmH, HHCallback(origin))
                    MatrixLog.i(TAG, "origin is ${origin?.javaClass?.name}")
                }
        }
    }

    fun hasForegroundService(): Boolean {
        if (activityManager == null) {
            throw IllegalStateException("NOT initialized yet")
        }
        return safeLet(TAG, defVal = false) {
            @Suppress("DEPRECATION")
            activityManager!!.getRunningServices(Int.MAX_VALUE)
                .filter {
                    it.uid == Process.myUid() && it.pid == Process.myPid()
                }.any {
                    it.foreground
                }
        }.also {
            if (!it) {
                fgServiceHandler?.clear()
            }
        }
    }

    private class HHCallback(private val mHCallback: Handler.Callback?) : Handler.Callback {

        private var reentrantFence = false

        override fun handleMessage(msg: Message): Boolean {
            if (reentrantFence) {
                MatrixLog.e(TAG, "reentrant!!! ignore this msg: ${msg.what}")
                return false
            }

            when (msg.what) {
                CREATE_SERVICE -> {
                    ActivityThreadmH?.post {
                        safeApply(TAG) {
                            injectAmIfNeeded()
                        }
                    }
                }
                STOP_SERVICE -> {
                    ActivityThreadmH?.post {
                        MatrixLifecycleThread.handler.post {
                            safeApply(TAG) {
                                hasForegroundService()
                            }
                        }
                    }
                }
            }

            reentrantFence = true
            val ret = mHCallback?.handleMessage(msg)
            reentrantFence = false

            return ret ?: false
        }

        private fun injectAmIfNeeded() {
            ActivityThreadmServices?.forEach {
                val targetAm = fieldServicemActivityManager?.get(it.value)
                if (Proxy.isProxyClass(targetAm!!.javaClass) && Proxy.getInvocationHandler(targetAm) == fgServiceHandler) {
                    return@forEach
                }
                if (fgServiceHandler == null) {
                    MatrixLog.i(TAG, "first inject")
                    fgServiceHandler = FgServiceHandler(targetAm)
                }
                MatrixLog.i(TAG, "going to inject ${it.value}")
                val s = it.value as Service
                checkIfAlreadyForegrounded(ComponentName(s, s.javaClass.name))
                fieldServicemActivityManager?.set(it.value, Proxy.newProxyInstance(targetAm.javaClass.classLoader, targetAm.javaClass.interfaces, fgServiceHandler!!))
            }
        }

        private fun checkIfAlreadyForegrounded(componentName: ComponentName) {
            MatrixLifecycleThread.handler.post {
                safeApply(TAG) {
                    activityManager?.getRunningServices(Int.MAX_VALUE)?.filter {
                        it.pid == Process.myPid()
                                && it.uid == Process.myUid()
                                && it.service == componentName
                                && it.foreground
                    }?.forEach {
                        MatrixLog.i(TAG, "service turned fg when create: ${it.service}")
                        fgServiceHandler?.onStartForeground(it.service)
                    }
                }
            }
        }
    }

    private class FgServiceHandler(val origin: Any?) : InvocationHandler {

        private val fgServiceRecord by lazy { HashSet<ComponentName>() }

        override fun invoke(proxy: Any?, method: Method?, vararg args: Any?): Any? {
            return try {
                val ret = method?.invoke(origin, *args)

                if (method?.name == "setServiceForeground") {
                    MatrixLog.i(TAG, "real invoked setServiceForeground: ${args.contentToString()}")
                    if (args.size > 3 && args[3] == null) {
                        onStopForeground(args[0] as ComponentName)
                    } else {
                        onStartForeground(args[0] as ComponentName)
                    }
                }

                ret
            } catch (e: Throwable) {
                MatrixLog.printErrStackTrace(TAG, e, "")
                null
            }
        }

        fun onStartForeground(componentName: ComponentName) {
            var shouldTurnOn = false
            synchronized(fgServiceRecord) {
                MatrixLog.i(TAG, "hack onStartForeground: $componentName")
                if (fgServiceRecord.isEmpty()) {
                    MatrixLog.i(TAG, "should turn ON")
                    shouldTurnOn = true
                }
                fgServiceRecord.add(componentName)
            }
            if (shouldTurnOn) {
                MatrixLog.i(TAG, "do turn ON")
                turnOn() // avoid holding lock of fgServiceRecord
            }
        }

        fun onStopForeground(componentName: ComponentName) {
            var shouldTurnOff = false
            synchronized(fgServiceRecord){
                MatrixLog.i(TAG, "hack onStopForeground: $componentName")
                fgServiceRecord.remove(componentName)
                if (fgServiceRecord.isEmpty()) {
                    MatrixLog.i(TAG, "should turn OFF")
                    shouldTurnOff = true
                }
            }
            if (shouldTurnOff) {
                MatrixLog.i(TAG, "do turn OFF")
                turnOff()
            }
        }

        fun clear() {
            var needTurnOff = false
            synchronized(fgServiceRecord) {
                if (fgServiceRecord.isNotEmpty()) {
                    fgServiceRecord.clear()
                    needTurnOff = true
                    MatrixLog.i(TAG, "clear done, should turn OFF")
                }
            }
            if (needTurnOff) {
                MatrixLog.i(TAG, "fix clear: do turn OFF")
                turnOff()
            }
        }
    }
}