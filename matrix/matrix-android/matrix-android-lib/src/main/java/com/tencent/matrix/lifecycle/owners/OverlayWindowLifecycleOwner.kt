package com.tencent.matrix.lifecycle.owners

import android.annotation.SuppressLint
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.view.View
import android.view.WindowManager
import com.tencent.matrix.lifecycle.StatefulOwner
import com.tencent.matrix.util.*

/**
 * Created by Yves on 2021/12/30
 */
@SuppressLint("PrivateApi")
object OverlayWindowLifecycleOwner : StatefulOwner() {

    private const val TAG = "Matrix.OverlayWindowLifecycleOwner"

    private val overlayViews = HashSet<Any>()
    private val runningHandler = MatrixHandlerThread.getDefaultHandler()
    private val mainHandler = Handler(Looper.getMainLooper())

    private var WindowManagerGlobal_mRoots: ArrayList<*>? = null

    private val Field_ViewRootImpl_mView by lazy {
        safeLetOrNull(TAG) {
            ReflectFiled<View>(Class.forName("android.view.ViewRootImpl"), "mView")
        }
    }

    internal fun init() {
        if (Build.VERSION.SDK_INT > 31) { // for safety
            MatrixLog.e(TAG, "NOT support for api-level ${Build.VERSION.SDK_INT} yet!!!")
            return
        }
        inject()
    }

    private val onViewRootChangedListener: ArrayListProxy.OnDataChangedListener by lazy {
        object : ArrayListProxy.OnDataChangedListener {
            override fun onAdded(o: Any) {
                // we post to main thread cause the View is NOT set to ViewRootImpl yet
                mainHandler.post {
                    val view = safeLetOrNull(TAG) {
                        Field_ViewRootImpl_mView?.get(o) as View
                    }
                    val layoutParams = view?.layoutParams
                    if (layoutParams is WindowManager.LayoutParams
                        && (layoutParams.type == WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                                || layoutParams.type == WindowManager.LayoutParams.TYPE_PHONE)
                    ) {
                        if (overlayViews.isEmpty()) {
                            runningHandler.post {
                                turnOn()
                            }
                        }
                        overlayViews.add(o)
                    }
                }
            }

            override fun onRemoved(o: Any) {
                overlayViews.remove(o)
                if (overlayViews.isEmpty()) {
                    runningHandler.post {
                        turnOff()
                    }
                }
            }
        }
    }

    @Suppress("LocalVariableName")
    private fun inject() = safeApply(TAG) {
        val Clazz_WindowManagerGlobal = Class.forName("android.view.WindowManagerGlobal")

        val WindowManagerGlobal_instance =
            ReflectUtils.invoke<Any>(Clazz_WindowManagerGlobal, "getInstance", null)

        val WindowManagerGlobal_mLock =
            ReflectUtils.get<Any>(Clazz_WindowManagerGlobal, "mLock", WindowManagerGlobal_instance)

        synchronized(WindowManagerGlobal_mLock) {
            val origin = ReflectUtils.get<ArrayList<*>>(
                Clazz_WindowManagerGlobal,
                "mRoots",
                WindowManagerGlobal_instance
            )
            val proxy = ArrayListProxy(origin, onViewRootChangedListener)
            ReflectUtils.set(Clazz_WindowManagerGlobal, "mRoots", WindowManagerGlobal_instance, proxy)
            WindowManagerGlobal_mRoots = proxy
        }
    }

    private var fallbacked = false

    fun hasVisibleWindow() = safeLet(TAG, log = true, defVal = false) {
        if (WindowManagerGlobal_mRoots == null) {
            if (fallbacked) {
                MatrixLog.e(TAG, "WindowManagerGlobal_mRoots not found")
                return@safeLet false
            }
            MatrixLog.i(TAG, "monitor disabled, fallback init")
            fallbacked = true
            WindowManagerGlobal_mRoots = safeLetOrNull(TAG) {
                Class.forName("android.view.WindowManagerGlobal").let {
                    val instance = ReflectUtils.invoke<Any>(it, "getInstance", null)
                    ReflectUtils.get(it, "mRoots", instance)
                }
            }
        }
        if (WindowManagerGlobal_mRoots == null) {
            MatrixLog.e(TAG, "WindowManagerGlobal_mRoots not found")
            return@safeLet false
        }
        if (Field_ViewRootImpl_mView == null) {
            MatrixLog.e(TAG, "Field_ViewRootImpl_mView not found")
            return@safeLet false
        }
        return@safeLet WindowManagerGlobal_mRoots!!.any {
            val v = Field_ViewRootImpl_mView!!.get(it)
            // windowVisibility is determined by app vibility
            // until the PRIVATE_FLAG_FORCE_DECOR_VIEW_VISIBILITY is set, which is blocked in Q
            return@any v != null && View.VISIBLE == v.visibility && View.VISIBLE == v.windowVisibility
        }
    }

    fun hasOverlayWindow() = active()

    /**
     * requires enable the monitor
     */
    internal fun getAllViews() = safeLet(TAG, log = true, defVal = emptyList<View>()) {
        return@safeLet WindowManagerGlobal_mRoots
            ?.map { Field_ViewRootImpl_mView!!.get(it) }
            ?.toList()
            ?: emptyList()
    }
}