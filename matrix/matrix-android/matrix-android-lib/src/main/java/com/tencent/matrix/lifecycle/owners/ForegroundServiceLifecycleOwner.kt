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
import com.tencent.matrix.lifecycle.StatefulOwner
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.safeApply
import com.tencent.matrix.util.safeLetOrNull
import java.lang.reflect.InvocationHandler
import java.lang.reflect.Method
import java.lang.reflect.Proxy

private val SDK_GUARD = 31

/**
 * Created by Yves on 2021/11/30
 */
object ForegroundServiceLifecycleOwner : StatefulOwner() {

    private const val TAG = "Matrix.lifecycle.FgService"

    private const val CREATE_SERVICE = 114

    @SuppressLint("DiscouragedPrivateApi")
    private val fieldServicemActivityManager = safeLetOrNull(TAG) {
        Class.forName("android.app.Service").getDeclaredField("mActivityManager")
            .apply { isAccessible = true }
    }

    private var activityManager: ActivityManager? = null
    private var ActivityThreadmServices: ArrayMap<*, *>? = null
    private var ActivityThreadmH: Handler? = null

    private var fgServiceHandler: FgServiceHandler? = null

    fun init(context: Context) {
        if (Build.VERSION.SDK_INT > SDK_GUARD) { // for safety
            MatrixLog.e(TAG, "NOT support for api-level ${Build.VERSION.SDK_INT} yet!!!")
            return
        }
        activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
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

    private class FgServiceHandler(val origin: Any?) : InvocationHandler {

        private val fgServiceRecord by lazy { HashSet<ComponentName>() }

        override fun invoke(proxy: Any?, method: Method?, vararg args: Any?): Any? {
            MatrixLog.d(TAG, "hack invoked : ${method?.name}, ${args.contentToString()}")
            return try {
                val ret = method?.invoke(origin, *args)

                if (method?.name == "setServiceForeground") {
                    MatrixLog.d(TAG, "real invoked setServiceForeground")
                    if (args.size == 6 && args[3] == null) {
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

        fun onStartForeground(componentName: ComponentName) = synchronized(fgServiceRecord) {
            MatrixLog.i(TAG, "hack onStartForeground: $componentName")
            if (fgServiceRecord.isEmpty()) {
                MatrixLog.i(TAG, "turn ON")
                turnOn()
            }
            fgServiceRecord.add(componentName)
        }

        fun onStopForeground(componentName: ComponentName) = synchronized(fgServiceRecord){
            MatrixLog.i(TAG, "hack onStopForeground: $componentName")
            fgServiceRecord.remove(componentName)
            if (fgServiceRecord.isEmpty()) {
                MatrixLog.i(TAG, "turn OFF")
                turnOff()
            }
        }
    }
}